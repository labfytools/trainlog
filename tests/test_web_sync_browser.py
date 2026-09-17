#!/usr/bin/env python3
"""Real Firefox click through the production C server and owned worker."""
import json, os, signal, socket, subprocess, tempfile, time, unittest, warnings
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/"tests/sync_orchestrator_peer_fixture.py"
GECKO=Path(os.environ.get("TRAINLOG_GECKODRIVER","/home/fy59/.cache/trainlog/web-e2e-tools/geckodriver"))
warnings.simplefilter("ignore", ResourceWarning)

class BrowserSyncTest(unittest.TestCase):
  def test_button_starts_worker_reconnects_and_displays_committed_draft(self):
    if not GECKO.is_file(): self.skipTest("pinned geckodriver unavailable")
    from selenium import webdriver
    from selenium.webdriver.common.by import By
    from selenium.webdriver.firefox.service import Service
    from selenium.webdriver.support.ui import WebDriverWait
    with tempfile.TemporaryDirectory(prefix="trainlog-web-sync-") as directory:
      root=Path(directory);xdg=root/"data";xdg.mkdir();config=root/"config.json"
      config.write_text(json.dumps({"format":"trainlog-sync-orchestrator-config","version":1,"enabled":True,"mode":"directory-test-double","capabilities":["generation-manifest-v1","generation-ack-v1","causal-delete-v1","mobile-history-v4","execution-draft-v1"],"peer_command":["python3",str(FIXTURE)],"timeout_seconds":30}))
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
        wait.until(lambda d:"Synchronisation confirmée" in d.page_source)
        summary=driver.find_element(By.XPATH,"//summary[contains(.,'Synchronisation confirmée')]");summary.click()
        wait.until(lambda d:"Brouillons reçus" in d.page_source and "pending" in d.page_source)
        driver.refresh();wait.until(lambda d:"Synchronisation confirmée" in d.page_source)
        state=json.loads(Path(str(xdg/"trainlog"/"trainlog.db")+".sync-run.json").read_text())
        self.assertEqual(state["phase"],"completed");self.assertEqual(state["sessions_reconciled"],1)
      finally:
        if driver is not None: driver.quit()
        os.killpg(server.pid,signal.SIGTERM)
        server.wait(timeout=10)

if __name__=="__main__":unittest.main()
