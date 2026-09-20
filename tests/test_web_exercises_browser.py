#!/usr/bin/env python3
"""Disposable real-browser smoke test for Web Exercises V1."""

import os
import signal
import socket
import subprocess
import tempfile
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build" / "tui"
GECKO = Path(os.environ.get(
    "TRAINLOG_GECKODRIVER", "/home/fy59/.cache/trainlog/web-e2e-tools/geckodriver"))


class BrowserExercisesTest(unittest.TestCase):
    def test_create_edit_retire_at_desktop_and_mobile_widths(self):
        if not GECKO.is_file():
            self.skipTest("pinned geckodriver unavailable")

        from selenium import webdriver
        from selenium.webdriver.common.by import By
        from selenium.webdriver.common.keys import Keys
        from selenium.webdriver.firefox.service import Service
        from selenium.common.exceptions import StaleElementReferenceException
        from selenium.webdriver.support.ui import Select, WebDriverWait

        with tempfile.TemporaryDirectory(prefix="trainlog-web-exercises-") as directory:
            temporary = Path(directory)
            data = temporary / "data"
            config = temporary / "config"
            data.mkdir()
            config.mkdir()
            with socket.socket() as reserved:
                reserved.bind(("127.0.0.1", 0))
                port = reserved.getsockname()[1]
            environment = os.environ.copy()
            environment.update({"XDG_DATA_HOME": str(data), "XDG_CONFIG_HOME": str(config),
                                "XDG_CACHE_HOME": str(temporary / "cache")})
            server = subprocess.Popen(
                [str(BUILD / "trainlog"), "--web", "--port", str(port)],
                env=environment, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                start_new_session=True,
            )
            driver = None
            try:
                for _ in range(200):
                    with socket.socket() as probe:
                        if probe.connect_ex(("127.0.0.1", port)) == 0:
                            break
                    time.sleep(0.025)
                options = webdriver.FirefoxOptions()
                options.add_argument("-headless")
                driver = webdriver.Firefox(options=options, service=Service(str(GECKO)))
                wait = WebDriverWait(driver, 15, ignored_exceptions=(StaleElementReferenceException,))
                driver.set_window_size(1440, 1000)
                driver.get(f"http://127.0.0.1:{port}/exercices")
                wait.until(lambda page: page.find_element(By.TAG_NAME, "h1").text == "Exercices")
                self.assertEqual(0, driver.execute_script(
                    "return document.documentElement.scrollWidth-document.documentElement.clientWidth"))

                driver.find_element(By.XPATH, "//button[contains(., 'Nouvel exercice')]").click()
                wait.until(lambda page: page.current_url.endswith("/exercices/nouveau"))
                name = wait.until(lambda page: page.find_element(By.CSS_SELECTOR, ".exercise-form input"))
                name.send_keys("Exercice visuel jetable")
                Select(driver.find_element(By.CSS_SELECTOR, ".exercise-form select")).select_by_index(1)
                name.send_keys(Keys.ENTER)
                wait.until(lambda page: page.find_element(By.TAG_NAME, "h1").text == "Exercice visuel jetable")

                driver.find_element(By.XPATH, "//button[normalize-space(.)='Modifier']").click()
                name = wait.until(lambda page: page.find_element(By.CSS_SELECTOR, ".exercise-form input"))
                name.clear()
                name.send_keys("Exercice visuel renommé")
                name.send_keys(Keys.ENTER)
                wait.until(lambda page: page.find_element(By.TAG_NAME, "h1").text == "Exercice visuel renommé")

                driver.set_window_size(390, 844)
                self.assertEqual(0, driver.execute_script(
                    "return document.documentElement.scrollWidth-document.documentElement.clientWidth"))
                driver.find_element(By.XPATH, "//button[contains(., 'Retirer du catalogue')]").click()
                wait.until(lambda page: page.find_element(By.CSS_SELECTOR, "[role='alertdialog']"))
                driver.find_element(By.XPATH, "//*[@role='alertdialog']//button[normalize-space(.)='Retirer']").click()
                wait.until(lambda page: page.find_element(By.TAG_NAME, "h1").text == "Exercices")
                self.assertNotIn("Exercice visuel renommé", driver.page_source)
            finally:
                if driver is not None:
                    driver.quit()
                os.killpg(server.pid, signal.SIGTERM)
                server.wait(timeout=10)


if __name__ == "__main__":
    unittest.main()
