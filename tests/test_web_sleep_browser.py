#!/usr/bin/env python3
"""Exercise the persisted Sleep Diary projection incrementally in Firefox."""

import json
import os
import signal
import socket
import subprocess
import tempfile
import time
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / os.environ.get("TRAINLOG_WEB_E2E_BUILD", "build") / "tui"
GECKO = Path(
    os.environ.get(
        "TRAINLOG_GECKODRIVER", "/home/fy59/.cache/trainlog/web-e2e-tools/geckodriver"
    )
)
EVIDENCE = ROOT / "docs" / "reviews" / "evidence"


class BrowserSleepDiaryTest(unittest.TestCase):
    def test_live_agenda_uses_each_persisted_revision_without_reload(self):
        if not GECKO.is_file():
            self.skipTest("pinned geckodriver unavailable")

        try:
            from selenium import webdriver
            from selenium.webdriver.common.by import By
            from selenium.webdriver.firefox.service import Service
            from selenium.webdriver.support.ui import Select, WebDriverWait
            import websocket
        except ModuleNotFoundError:
            self.skipTest("selenium unavailable")

        with tempfile.TemporaryDirectory(prefix="trainlog-web-sleep-") as directory:
            root = Path(directory)
            environment = os.environ.copy()
            environment.update(
                {
                    "XDG_DATA_HOME": str(root / "data"),
                    "XDG_CONFIG_HOME": str(root / "config"),
                    "XDG_CACHE_HOME": str(root / "cache"),
                }
            )
            with socket.socket() as reserved:
                reserved.bind(("127.0.0.1", 0))
                port = reserved.getsockname()[1]
            server = subprocess.Popen(
                [str(BUILD / "trainlog"), "--web", "--port", str(port)],
                env=environment,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
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
                options.set_capability("webSocketUrl", True)
                driver = webdriver.Firefox(options=options, service=Service(str(GECKO)))
                driver.set_window_size(1440, 1100)
                wait = WebDriverWait(driver, 15)
                driver.get(f"http://127.0.0.1:{port}/analyse?section=sleep")
                wait.until(
                    lambda current: current.find_element(
                        By.CSS_SELECTOR, "[data-testid='sleep-workspace']"
                    )
                )
                driver.execute_script(
                    """
                    window.__trainlogErrors = [];
                    window.__trainlogFetches = [];
                    addEventListener('error', event => window.__trainlogErrors.push(String(event.error || event.message)));
                    addEventListener('unhandledrejection', event => window.__trainlogErrors.push(String(event.reason)));
                    const originalFetch = window.fetch;
                    window.fetch = async (...args) => {
                      try {
                        const response = await originalFetch(...args);
                        window.__trainlogFetches.push({url: String(args[0]), method: args[1]?.method || 'GET', status: response.status});
                        return response;
                      } catch (error) {
                        window.__trainlogErrors.push(String(error));
                        throw error;
                      }
                    };
                    """
                )

                def set_value(test_id: str, value: str):
                    element = driver.find_element(
                        By.CSS_SELECTOR, f"[data-testid='{test_id}']"
                    )
                    driver.execute_script(
                        "Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(arguments[0],arguments[1]);"
                        "arguments[0].dispatchEvent(new Event('input',{bubbles:true}));"
                        "arguments[0].dispatchEvent(new Event('change',{bubbles:true}));",
                        element,
                        value,
                    )

                def click(element):
                    driver.execute_script(
                        "arguments[0].scrollIntoView({block:'center'}); arguments[0].click();",
                        element,
                    )

                def post_count() -> int:
                    return driver.execute_script(
                        "return window.__trainlogFetches.filter(item => item.method === 'POST').length"
                    )

                def await_persisted(previous_posts: int):
                    wait.until(lambda current: post_count() > previous_posts)
                    wait.until(
                        lambda current: current.find_element(
                            By.CSS_SELECTOR, "[data-testid='sleep-save-state']"
                        ).text
                        == "Enregistré localement"
                    )

                def add_medication(dose: str):
                    click(driver.find_element(
                        By.XPATH,
                        "//button[normalize-space()='+ Ajouter un médicament']",
                    ))
                    set_value("sleep-medication-name", "venlafaxine")
                    set_value("sleep-medication-dose", dose)
                    before = post_count()
                    click(driver.find_element(
                        By.CSS_SELECTOR, "[data-testid='sleep-add-medication']"
                    ))
                    wait.until(lambda current: post_count() > before)
                    wait.until(
                        lambda current: not current.find_elements(
                            By.CSS_SELECTOR, "[data-testid='sleep-medication-name']"
                        )
                    )

                add_medication("75")
                add_medication("37.5")
                medication_select = Select(
                    driver.find_element(
                        By.CSS_SELECTOR, "[data-testid='sleep-intake-medication']"
                    )
                )
                option_labels = [option.text for option in medication_select.options]
                self.assertIn("venlafaxine — 75 mg", option_labels)
                self.assertIn("venlafaxine — 37,5 mg", option_labels)

                for label, dose in (
                    ("venlafaxine — 75 mg", "75"),
                    ("venlafaxine — 37,5 mg", "37.5"),
                ):
                    medication_select.select_by_visible_text(label)
                    wait.until(
                        lambda current: current.find_element(
                            By.CSS_SELECTOR, "[data-testid='sleep-intake-dose']"
                        ).get_attribute("value")
                        == dose
                    )
                    before = post_count()
                    click(driver.find_element(
                        By.CSS_SELECTOR, "[data-testid='sleep-add-intake']"
                    ))
                    await_persisted(before)
                wait.until(
                    lambda current: current.find_element(
                        By.CSS_SELECTOR, "[data-testid='sleep-agenda-medication']"
                    ).text
                    == "M ×2"
                )

                def add_event(kind: str, start: str, end: str | None, assertion):
                    Select(
                        driver.find_element(
                            By.CSS_SELECTOR, "[data-testid='sleep-event-type']"
                        )
                    ).select_by_value(kind)
                    set_value("sleep-event-start", start)
                    if end is not None:
                        set_value("sleep-event-end", end)
                    before = post_count()
                    click(driver.find_element(
                        By.CSS_SELECTOR, "[data-testid='sleep-add-event']"
                    ))
                    await_persisted(before)
                    wait.until(assertion)

                add_event(
                    "bed_time",
                    "22:45",
                    None,
                    lambda current: current.find_element(
                        By.CSS_SELECTOR, "[data-testid='sleep-agenda-bed-time']"
                    ).text.endswith("22:45"),
                )
                add_event(
                    "sleep",
                    "23:15",
                    "03:00",
                    lambda current: "3 h 45 min"
                    in current.find_element(
                        By.CSS_SELECTOR, "[data-testid='sleep-agenda-sleep-duration']"
                    ).text,
                )
                add_event(
                    "long_awake",
                    "03:00",
                    "03:30",
                    lambda current: "1 · 30 min"
                    in current.find_element(
                        By.CSS_SELECTOR, "[data-testid='sleep-agenda-long-awake']"
                    ).text,
                )
                add_event(
                    "sleep",
                    "03:30",
                    "06:45",
                    lambda current: "7 h 00 min"
                    in current.find_element(
                        By.CSS_SELECTOR, "[data-testid='sleep-agenda-sleep-duration']"
                    ).text,
                )
                add_event(
                    "final_get_up",
                    "07:10",
                    None,
                    lambda current: "8 h 25 min"
                    in current.find_element(
                        By.CSS_SELECTOR, "[data-testid='sleep-agenda-time-in-bed']"
                    ).text,
                )

                expected = driver.find_element(
                    By.CSS_SELECTOR, "[data-testid^='sleep-agenda-row-']"
                ).text
                mutation_errors = driver.execute_script(
                    "return window.__trainlogErrors"
                )
                mutation_fetches = driver.execute_script(
                    "return window.__trainlogFetches"
                )
                self.assertEqual([], mutation_errors)
                self.assertTrue(mutation_fetches)
                self.assertTrue(
                    all(item["status"] == 200 for item in mutation_fetches)
                )
                driver.refresh()
                wait.until(
                    lambda current: "8 h 25 min"
                    in current.find_element(
                        By.CSS_SELECTOR, "[data-testid='sleep-agenda-time-in-bed']"
                    ).text
                )
                actual = driver.find_element(
                    By.CSS_SELECTOR, "[data-testid^='sleep-agenda-row-']"
                ).text
                self.assertEqual(expected, actual)
                self.assertTrue(
                    driver.find_element(
                        By.CSS_SELECTOR, "[data-testid='sleep-workspace']"
                    ).is_displayed()
                )
                self.assertLessEqual(
                    driver.execute_script("return document.documentElement.scrollWidth"),
                    driver.execute_script("return document.documentElement.clientWidth"),
                )
                bidi = websocket.create_connection(
                    driver.capabilities["webSocketUrl"], suppress_origin=True
                )
                bidi.send(
                    json.dumps(
                        {"id": 1, "method": "browsingContext.getTree", "params": {}}
                    )
                )
                tree = json.loads(bidi.recv())
                context = tree["result"]["contexts"][0]["context"]
                bidi.send(
                    json.dumps(
                        {
                            "id": 2,
                            "method": "browsingContext.setViewport",
                            "params": {
                                "context": context,
                                "viewport": {"width": 390, "height": 844},
                                "devicePixelRatio": 1,
                            },
                        }
                    )
                )
                response = json.loads(bidi.recv())
                self.assertEqual("success", response["type"])
                wait.until(lambda current: current.execute_script("return innerWidth") == 390)
                self.assertLessEqual(
                    driver.execute_script("return document.documentElement.scrollWidth"),
                    driver.execute_script("return document.documentElement.clientWidth"),
                )
                bidi.send(
                    json.dumps(
                        {
                            "id": 3,
                            "method": "browsingContext.setViewport",
                            "params": {
                                "context": context,
                                "viewport": {"width": 1440, "height": 1100},
                                "devicePixelRatio": 1,
                            },
                        }
                    )
                )
                self.assertEqual("success", json.loads(bidi.recv())["type"])
                bidi.close()
                EVIDENCE.mkdir(parents=True, exist_ok=True)
                self.assertTrue(
                    driver.save_screenshot(
                        str(EVIDENCE / "sleep-diary-live-agenda-firefox.png")
                    )
                )
            finally:
                if driver is not None:
                    driver.quit()
                try:
                    os.killpg(server.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                server.wait(timeout=10)


if __name__ == "__main__":
    unittest.main()
