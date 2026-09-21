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
import uuid
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / os.environ.get("TRAINLOG_WEB_E2E_BUILD", "build") / "tui"
EXECUTABLE = Path(os.environ.get("TRAINLOG_WEB_E2E_EXECUTABLE", BUILD / "trainlog"))
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
            configured_port = os.environ.get("TRAINLOG_WEB_E2E_PORT")
            if configured_port is None:
                with socket.socket() as reserved:
                    reserved.bind(("127.0.0.1", 0))
                    port = reserved.getsockname()[1]
            else:
                port = int(configured_port)
            server = subprocess.Popen(
                [str(EXECUTABLE), "--web", "--port", str(port)],
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

                EVIDENCE.mkdir(parents=True, exist_ok=True)
                options = webdriver.FirefoxOptions()
                options.add_argument("-headless")
                options.set_capability("webSocketUrl", True)
                options.set_preference("browser.download.folderList", 2)
                options.set_preference("browser.download.dir", str(EVIDENCE))
                options.set_preference("browser.download.useDownloadDir", True)
                options.set_preference(
                    "browser.helperApps.neverAsk.saveToDisk", "application/pdf"
                )
                options.set_preference("pdfjs.disabled", True)
                driver = webdriver.Firefox(options=options, service=Service(str(GECKO)))
                driver.set_window_size(1440, 1100)
                wait = WebDriverWait(driver, 15)
                base_url = os.environ.get(
                    "TRAINLOG_WEB_E2E_BASE_URL", f"http://127.0.0.1:{port}"
                )
                driver.get(f"{base_url}/analyse?section=sleep")
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

                def stable_id(prefix: str) -> str:
                    return f"{prefix}_{uuid.uuid4()}"

                def post_entries(entries: list[dict]):
                    return driver.execute_async_script(
                        """
                        const entries=arguments[0], done=arguments[arguments.length-1];
                        (async () => {
                          const status = await fetch('/api/v1/sync/status');
                          const token = status.headers.get('X-Trainlog-CSRF-Token');
                          const responses = [];
                          for (const entry of entries) {
                            const response = await fetch('/api/v1/sleep-diary', {
                              method: 'POST',
                              headers: {'Accept':'application/json','Content-Type':'application/json','X-Trainlog-CSRF-Token':token},
                              body: JSON.stringify(entry),
                            });
                            responses.push({status: response.status, body: await response.json()});
                          }
                          done(responses);
                        })().catch(error => done([{status:0, body:String(error)}]));
                        """,
                        entries,
                    )

                alignment_medication_id = Select(
                    driver.find_element(
                        By.CSS_SELECTOR, "[data-testid='sleep-intake-medication']"
                    )
                ).options[1].get_attribute("value")
                alignment_entry = {
                    "entry_id": "",
                    "expected_revision": None,
                    "night_start_date": "2026-09-19",
                    "night_end_date": "2026-09-20",
                    "created_at": "2026-09-19T18:00:00+02:00",
                    "updated_at": "2026-09-20T16:00:00+02:00",
                    "sleep_quality": "B",
                    "wake_quality": "TB",
                    "day_form": "Moy",
                    "treatment_and_notes": "Nuit de contrôle géométrique.",
                    "events": [
                        {
                            "event_id": stable_id("sle"),
                            "type": "bed_time",
                            "start_at": "2026-09-19T22:30:00+02:00",
                            "end_at": None,
                        },
                        {
                            "event_id": stable_id("sle"),
                            "type": "sleep",
                            "start_at": "2026-09-19T22:30:00+02:00",
                            "end_at": "2026-09-20T03:30:00+02:00",
                        },
                        {
                            "event_id": stable_id("sle"),
                            "type": "final_get_up",
                            "start_at": "2026-09-20T04:45:00+02:00",
                            "end_at": None,
                        },
                        {
                            "event_id": stable_id("sle"),
                            "type": "daytime_sleepiness",
                            "start_at": "2026-09-20T15:30:00+02:00",
                            "end_at": None,
                        },
                    ],
                    "intakes": [
                        {
                            "intake_id": stable_id("mdi"),
                            "medication_id": alignment_medication_id,
                            "medication_name": "venlafaxine",
                            "taken_at": "2026-09-19T22:30:00+02:00",
                            "dose_value": 75,
                            "dose_unit": "mg",
                            "note": "",
                            "created_at": "2026-09-19T22:30:00+02:00",
                        }
                    ],
                }
                responses = post_entries([alignment_entry])
                self.assertEqual([200], [item["status"] for item in responses])
                set_value("sleep-night-start", "2026-09-19")
                wait.until(
                    lambda current: current.find_elements(
                        By.CSS_SELECTOR, "[data-testid='sleep-agenda-event-bed_time']"
                    )
                )
                geometry = driver.execute_script(
                    """
                    const axis=document.querySelector('.sleep-hour-axis').getBoundingClientRect();
                    const box=id => document
                      .querySelector(`[data-testid='${id}']`)
                      .getBoundingClientRect();
                    const point=id => {
                      const value=document
                        .querySelector(`[data-testid='${id}'] > span`)
                        .getBoundingClientRect();
                      return value.left + value.width / 2;
                    };
                    const sleep=box('sleep-agenda-event-sleep');
                    const ticks=[...document.querySelectorAll('.sleep-hour-axis > span')]
                      .map(value => {
                        const tick=value.getBoundingClientRect();
                        return {
                          label:value.textContent,
                          center:tick.left + tick.width / 2,
                        };
                      });
                    return {
                      left:axis.left,
                      width:axis.width,
                      bed:point('sleep-agenda-event-bed_time'),
                      medication:point('sleep-agenda-medication'),
                      sleepLeft:sleep.left,
                      sleepRight:sleep.right,
                      getUp:point('sleep-agenda-event-final_get_up'),
                      sleepiness:point('sleep-agenda-event-daytime_sleepiness'),
                      ticks,
                    };
                    """
                )
                expected_positions = {
                    "bed": 4.5 / 24,
                    "medication": 4.5 / 24,
                    "sleepLeft": 4.5 / 24,
                    "sleepRight": 9.5 / 24,
                    "getUp": 10.75 / 24,
                    "sleepiness": 21.5 / 24,
                }
                for key, fraction in expected_positions.items():
                    expected_x = geometry["left"] + geometry["width"] * fraction
                    self.assertAlmostEqual(
                        expected_x, geometry[key], delta=1.0, msg=key
                    )
                for tick_index in (4, 9, 10, 11, 21, 22):
                    expected_x = geometry["left"] + geometry["width"] * tick_index / 24
                    self.assertAlmostEqual(
                        expected_x,
                        geometry["ticks"][tick_index]["center"],
                        delta=1.0,
                    )
                self.assertTrue(
                    driver.save_screenshot(
                        str(EVIDENCE / "sleep-diary-alignment-firefox.png")
                    )
                )

                set_value("sleep-night-start", "2026-09-21")
                wait.until(
                    lambda current: "Aucune donnée pour cette nuit."
                    in current.page_source
                )
                self.assertFalse(
                    driver.find_elements(
                        By.CSS_SELECTOR,
                        "[data-testid='sleep-timeline'] [data-testid^='sleep-event-']",
                    )
                )
                self.assertFalse(
                    driver.find_elements(By.CSS_SELECTOR, "[data-testid='sleep-intake-row']")
                )
                self.assertTrue(
                    driver.save_screenshot(
                        str(EVIDENCE / "sleep-diary-empty-night-firefox.png")
                    )
                )
                set_value("sleep-night-start", "2026-09-19")
                wait.until(
                    lambda current: current.find_elements(
                        By.CSS_SELECTOR, "[data-testid='sleep-event-bed_time']"
                    )
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
                self.assertTrue(
                    driver.save_screenshot(
                        str(EVIDENCE / "sleep-diary-live-agenda-firefox.png")
                    )
                )

                extra_entries = []
                for day in range(14, 19):
                    start_date = f"2026-09-{day:02d}"
                    end_date = f"2026-09-{day + 1:02d}"

                    def at(clock: str) -> str:
                        date = start_date if int(clock[:2]) >= 18 else end_date
                        return f"{date}T{clock}:00+02:00"

                    extra_entries.append(
                        {
                            "entry_id": "",
                            "expected_revision": None,
                            "night_start_date": start_date,
                            "night_end_date": end_date,
                            "created_at": at("18:00"),
                            "updated_at": at("18:00"),
                            "sleep_quality": "B",
                            "wake_quality": "Moy",
                            "day_form": "TB",
                            "treatment_and_notes": "Remarque française synthétique.",
                            "events": [
                                {
                                    "event_id": stable_id("sle"),
                                    "type": "bed_time",
                                    "start_at": at("22:30"),
                                    "end_at": None,
                                },
                                {
                                    "event_id": stable_id("sle"),
                                    "type": "sleep",
                                    "start_at": at("23:00"),
                                    "end_at": at("03:00"),
                                },
                                {
                                    "event_id": stable_id("sle"),
                                    "type": "long_awake",
                                    "start_at": at("03:00"),
                                    "end_at": at("03:45"),
                                },
                                {
                                    "event_id": stable_id("sle"),
                                    "type": "sleep",
                                    "start_at": at("03:45"),
                                    "end_at": at("07:00"),
                                },
                                {
                                    "event_id": stable_id("sle"),
                                    "type": "half_sleep",
                                    "start_at": at("07:00"),
                                    "end_at": at("07:10"),
                                },
                                {
                                    "event_id": stable_id("sle"),
                                    "type": "final_get_up",
                                    "start_at": at("07:15"),
                                    "end_at": None,
                                },
                                {
                                    "event_id": stable_id("sle"),
                                    "type": "nap",
                                    "start_at": at("14:00"),
                                    "end_at": at("14:35"),
                                },
                                {
                                    "event_id": stable_id("sle"),
                                    "type": "daytime_sleepiness",
                                    "start_at": at("15:00"),
                                    "end_at": None,
                                },
                            ],
                            "intakes": [],
                        }
                    )
                responses = post_entries(extra_entries)
                self.assertEqual([200] * 5, [item["status"] for item in responses])
                driver.refresh()
                wait.until(
                    lambda current: len(
                        current.find_elements(
                            By.CSS_SELECTOR, "[data-testid^='sleep-agenda-row-']"
                        )
                    )
                    == 1
                )
                EVIDENCE.mkdir(parents=True, exist_ok=True)
                download_path = EVIDENCE / "trainlog-sleep-diary.pdf"
                pdf_path = EVIDENCE / "sleep-diary-fr-7-days.pdf"
                raster_prefix = EVIDENCE / "sleep-diary-fr-7-days"
                raster_path = EVIDENCE / "sleep-diary-fr-7-days.png"
                numbered_download = EVIDENCE / "trainlog-sleep-diary(1).pdf"
                for artifact in (
                    download_path,
                    numbered_download,
                    pdf_path,
                    raster_path,
                ):
                    if artifact.exists():
                        artifact.unlink()
                click(driver.find_element(By.XPATH, "//button[normalize-space()='Exporter PDF']"))
                wait.until(
                    lambda current: download_path.is_file()
                    and download_path.stat().st_size > 0
                )
                download_path.replace(pdf_path)
                extracted = root / "sleep-diary-fr-7-days.txt"
                subprocess.run(
                    ["pdftotext", str(pdf_path), str(extracted)], check=True
                )
                pdf_text = extracted.read_text(encoding="utf-8")
                self.assertIn("QUALITÉ DU", pdf_text)
                self.assertIn("LONG RÉVEIL", pdf_text.upper())
                self.assertIn("AVERTISSEMENT : 7 jours non validés", pdf_text)
                for forbidden in (
                    "WAKE",
                    "TREATMENT",
                    "AWAKE",
                    "sleepiness",
                    "medication intake",
                    "nights",
                ):
                    self.assertNotIn(forbidden, pdf_text)
                subprocess.run(
                    [
                        "pdftoppm",
                        "-png",
                        "-f",
                        "1",
                        "-singlefile",
                        "-r",
                        "150",
                        str(pdf_path),
                        str(raster_prefix),
                    ],
                    check=True,
                )
                bidi.close()
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
