#!/usr/bin/env python3
"""Real Firefox and Android production services in one correlated conversation."""
import json, os, signal, socket, sqlite3, subprocess, tempfile, time, unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
GECKO=Path(os.environ.get("TRAINLOG_GECKODRIVER","/home/fy59/.cache/trainlog/web-e2e-tools/geckodriver"))

def await_condition(predicate, seconds=30):
  deadline=time.monotonic()+seconds
  while time.monotonic()<deadline:
    value=predicate()
    if value: return value
    time.sleep(.025)
  raise AssertionError("condition did not become true")

class BrowserSyncTest(unittest.TestCase):
  def test_button_starts_worker_reconnects_and_displays_committed_draft(self):
    if not GECKO.is_file(): self.skipTest("pinned geckodriver unavailable")
    from selenium import webdriver
    from selenium.webdriver.common.by import By
    from selenium.webdriver.firefox.service import Service
    from selenium.webdriver.support.ui import WebDriverWait
    with tempfile.TemporaryDirectory(prefix="trainlog-web-sync-") as directory:
      root=Path(directory);xdg=root/"data";xdg.mkdir();config=root/"config.json"
      transport=root/"transport";transport.mkdir();owned=root/"owned";owned.mkdir()
      release=root/"release-android";android_log=(root/"android.log").open("wb")
      android_env=os.environ.copy();android_env.update({"JAVA_HOME":"/usr/lib/jvm/java-17-openjdk","TRAINLOG_SYNC_TRANSPORT_ROOT":str(transport),"TRAINLOG_SYNC_TEST_RELEASE_FILE":str(release)})
      android=subprocess.Popen(["./gradlew","--no-daemon","testDebugUnitTest","--tests","com.labfytools.trainlog.data.SyncDeploymentConversationTest","--rerun-tasks"],cwd=ROOT/"android",env=android_env,stdout=android_log,stderr=subprocess.STDOUT,start_new_session=True)
      try:
        peer_file=await_condition(lambda: (transport/"android-peer-v1.json") if (transport/"android-peer-v1.json").is_file() else None,120)
      except Exception as error:
        if android.poll() is None: os.killpg(android.pid,signal.SIGTERM)
        android.wait(timeout=10);android_log.close()
        raise AssertionError(f"Android bridge did not advertise:\n{(root/'android.log').read_text(errors='replace')}") from error
      peer=json.loads(peer_file.read_text())
      config.write_text(json.dumps({"format":"trainlog-sync-orchestrator-config","version":1,"enabled":True,"mode":"directory","expected_peer_id":peer["peer_id"],"transport_root":str(transport),"owned_root":str(owned),"timeout_seconds":30}))
      with socket.socket() as reserved:
        reserved.bind(("127.0.0.1",0));port=reserved.getsockname()[1]
      env=os.environ.copy();env.update({"XDG_DATA_HOME":str(xdg),"XDG_CONFIG_HOME":str(root/"config"),"XDG_CACHE_HOME":str(root/"cache"),"TRAINLOG_SYNC_GENERATION_CONFIG":str(config)})
      server=subprocess.Popen([str(ROOT/"build/tui/trainlog"),"--web","--port",str(port)],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,start_new_session=True)
      driver=None
      try:
        for _ in range(100):
          with socket.socket() as probe:
            if probe.connect_ex(("127.0.0.1",port))==0: break
          time.sleep(.05)
        options=webdriver.FirefoxOptions();options.add_argument("-headless")
        driver=webdriver.Firefox(options=options,service=Service(str(GECKO)))
        driver.get(f"http://127.0.0.1:{port}/")
        wait=WebDriverWait(driver,15)
        button=wait.until(lambda d:d.find_element(By.XPATH,"//button[normalize-space()='Synchroniser']"))
        wait.until(lambda _ : button.is_enabled());button.click()
        state_path=Path(str(xdg/"trainlog"/"trainlog.db")+".sync-run.json")
        committed=await_condition(lambda: json.loads(state_path.read_text()) if state_path.is_file() and json.loads(state_path.read_text()).get("phase")=="local_import_committed" else None)
        self.assertEqual(committed["result"],"running")
        with sqlite3.connect(xdg/"trainlog"/"trainlog.db") as database:
          self.assertEqual(database.execute("SELECT COUNT(*) FROM execution_drafts").fetchone()[0],1)
        release.touch()
        final=await_condition(lambda: json.loads(state_path.read_text()) if json.loads(state_path.read_text()).get("phase") in ("completed","failed","interrupted") else None)
        self.assertEqual(final["phase"],"completed",f"state={final}\nandroid={((root/'android.log').read_text(errors='replace'))}")
        wait.until(lambda d:"Synchronisation confirmée" in d.page_source)
        summary=driver.find_element(By.XPATH,"//summary[contains(.,'Synchronisation confirmée')]");summary.click()
        wait.until(lambda d:"Brouillons reçus" in d.page_source and "active" in d.page_source)
        driver.refresh();wait.until(lambda d:"Synchronisation confirmée" in d.page_source)
        state=json.loads(Path(str(xdg/"trainlog"/"trainlog.db")+".sync-run.json").read_text())
        self.assertEqual(state["phase"],"completed")
        self.assertEqual(android.wait(timeout=30),0,(root/"android.log").read_text(errors="replace"))
      finally:
        if driver is not None: driver.quit()
        os.killpg(server.pid,signal.SIGTERM)
        server.wait(timeout=10)
        if android.poll() is None:
          os.killpg(android.pid,signal.SIGTERM);android.wait(timeout=10)
        android_log.close()

if __name__=="__main__":unittest.main()
