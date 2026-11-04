"""Offline tests: planning sync is idempotent and preserves human edits."""
import copy
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("plan_sync", ROOT / "scripts/sync_project_plan.py")
sync = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sync)


class PlanSyncTests(unittest.TestCase):
    def setUp(self):
        self.plan = json.loads((ROOT / ".github/project-plan.json").read_text())
        self.milestones = []
        self.labels = [{"name": "keep-my-label"}]
        self.issues = {
            item["number"]: {
                "body": f"<!-- cn-plan:{item['plan_id']} -->\n- **Dependencies:** placeholder\n- [x] Human progress stays checked\n",
                "assignees": [{"login": item["assignee"]}],
                "labels": [{"name": "keep-my-label"}],
                "milestone": None,
            } for item in self.plan["issues"]
        }
        self.writes = 0
        self.original_api = sync.api
        sync.api = self.api

    def tearDown(self):
        sync.api = self.original_api

    def api(self, method, path, payload=None):
        route = path.split("?")[0]
        if method != "GET":
            self.writes += 1
        if route == "/milestones":
            if method == "GET":
                return copy.deepcopy(self.milestones)
            item = dict(payload, number=len(self.milestones) + 1)
            self.milestones.append(item)
            return copy.deepcopy(item)
        if route == "/labels":
            if method == "GET":
                return copy.deepcopy(self.labels)
            self.labels.append(dict(payload))
            return dict(payload)
        if route.startswith("/issues/"):
            current = self.issues[int(route.rsplit("/", 1)[1])]
            if method == "PATCH":
                for key, value in payload.items():
                    if key == "milestone":
                        current[key] = {"number": value}
                    elif key == "labels":
                        current[key] = [{"name": name} for name in value]
                    elif key == "assignees":
                        current[key] = [{"login": name} for name in value]
                    else:
                        current[key] = value
            return copy.deepcopy(current)
        raise AssertionError(f"Unexpected request: {method} {path}")

    def test_sync_twice_preserves_edits_and_does_not_duplicate(self):
        sync.sync(self.plan)
        self.assertEqual(len(self.milestones), 5)
        for issue in self.issues.values():
            self.assertIn("- [x] Human progress stays checked", issue["body"])
            self.assertIn({"name": "keep-my-label"}, issue["labels"])
            self.assertNotIn("placeholder", issue["body"])
            self.assertIsNotNone(issue["milestone"])
        writes = self.writes
        sync.sync(self.plan)
        self.assertEqual(self.writes, writes)

    def test_duplicate_issue_mapping_rejected(self):
        self.plan["issues"][1]["number"] = self.plan["issues"][0]["number"]
        with self.assertRaises(ValueError):
            sync.validate(self.plan)
        self.assertEqual(self.writes, 0)

    def test_cycles_rejected(self):
        self.plan["issues"][0]["dependencies"] = [self.plan["issues"][1]["plan_id"]]
        self.plan["issues"][1]["dependencies"] = [self.plan["issues"][0]["plan_id"]]
        with self.assertRaises(ValueError):
            sync.validate(self.plan)

    def test_wrong_remote_issue_marker_rejected(self):
        self.issues[self.plan["issues"][0]["number"]]["body"] = "Unrelated human issue"
        with self.assertRaises(ValueError):
            sync.sync(self.plan)
        self.assertEqual(self.issues[self.plan["issues"][0]["number"]]["body"], "Unrelated human issue")


if __name__ == "__main__":
    unittest.main()
