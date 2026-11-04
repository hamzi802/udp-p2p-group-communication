#!/usr/bin/env python3
"""Sync only this repository's five milestones and existing proposal issues.

Uses the short-lived Actions token with contents:read and issues:write.
It does not create issues, delete data, edit code, grant access or change rules.
"""
import argparse
import json
import os
from pathlib import Path
import re
from urllib.error import HTTPError
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]
EXPECTED_REPOSITORY = "hamzi802/udp-p2p-group-communication"


def validate(plan):
    if plan["repository"] != EXPECTED_REPOSITORY:
        raise ValueError("Unexpected target repository")
    milestones = {m["key"] for m in plan["milestones"]}
    if len(plan["milestones"]) != 5 or milestones != {"MS1", "MS2", "MS3", "MS4", "MS5"}:
        raise ValueError("Expected five unique proposal milestones")
    issues = plan["issues"]
    ids = [i["plan_id"] for i in issues]
    numbers = [i["number"] for i in issues]
    if len(ids) != 44 or len(set(ids)) != 44 or len(set(numbers)) != 44:
        raise ValueError("Expected 44 unique mapped issues")
    known = set(ids)
    for item in issues:
        if item["milestone"] not in milestones or item["assignee"] != plan["roles"][item["role"]]:
            raise ValueError("Invalid milestone or role assignment")
        if not isinstance(item["number"], int) or item["number"] <= 0:
            raise ValueError("Invalid GitHub issue number")
        if not set(item["dependencies"]).issubset(known) or item["plan_id"] in item["dependencies"]:
            raise ValueError("Invalid dependency")
    # Detect dependency cycles before publishing metadata.
    lookup = {i["plan_id"]: i for i in issues}
    visiting, done = set(), set()
    def visit(key):
        if key in visiting:
            raise ValueError("Dependency cycle")
        if key in done:
            return
        visiting.add(key)
        for dep in lookup[key]["dependencies"]:
            visit(dep)
        visiting.remove(key)
        done.add(key)
    for key in ids:
        visit(key)


def api(method, path, payload=None):
    token = os.environ["GITHUB_TOKEN"]
    data = None if payload is None else json.dumps(payload).encode()
    request = Request(
        "https://api.github.com/repos/" + EXPECTED_REPOSITORY + path,
        data=data, method=method,
        headers={"Authorization": "Bearer " + token,
                 "Accept": "application/vnd.github+json",
                 "X-GitHub-Api-Version": "2022-11-28",
                 "Content-Type": "application/json",
                 "User-Agent": "cn-project-plan-sync"})
    try:
        with urlopen(request, timeout=60) as response:
            body = response.read()
            return json.loads(body) if body else None
    except HTTPError as error:
        # Do not print headers or the token.
        message = error.read().decode(errors="replace")
        raise RuntimeError(f"GitHub {method} {path}: {error.code} {message}") from None


def pages(path):
    result, page = [], 1
    separator = "&" if "?" in path else "?"
    while True:
        batch = api("GET", f"{path}{separator}per_page=100&page={page}")
        result.extend(batch)
        if len(batch) < 100:
            return result
        page += 1


def sync(plan):
    validate(plan)
    existing = pages("/milestones?state=all")
    milestone_numbers = {}
    for item in plan["milestones"]:
        matches = [m for m in existing if m["title"] == item["title"]]
        if len(matches) > 1:
            raise ValueError("Duplicate milestone title; resolve manually")
        description = (item["description"] +
                       "\n\nProposed internal deadline, end of day Asia/Karachi; not a confirmed instructor submission deadline.")
        payload = {"title": item["title"], "description": description, "due_on": item["due_on"]}
        if not matches:
            current = api("POST", "/milestones", payload)
        else:
            current = matches[0]
            if any(current.get(key) != value for key, value in payload.items()):
                current = api("PATCH", f"/milestones/{current['number']}", payload)
        milestone_numbers[item["key"]] = current["number"]

    label_specs = {"task": "1d76db", "optional": "c5def5",
                   "role:m1": "0052cc", "role:m2": "0e8a16",
                   "role:m3": "fbca04", "role:m4": "5319e7"}
    labels = {label["name"] for label in pages("/labels")}
    for name, color in label_specs.items():
        if name not in labels:
            api("POST", "/labels", {"name": name, "color": color})

    numbers = {i["plan_id"]: i["number"] for i in plan["issues"]}
    updated = 0
    for item in plan["issues"]:
        path = f"/issues/{item['number']}"
        current = api("GET", path)
        marker = f"<!-- cn-plan:{item['plan_id']} -->"
        if current.get("pull_request") or marker not in (current.get("body") or ""):
            raise ValueError(f"Wrong issue mapping for {item['plan_id']}")
        payload = {}
        wanted_milestone = milestone_numbers[item["milestone"]]
        if (current.get("milestone") or {}).get("number") != wanted_milestone:
            payload["milestone"] = wanted_milestone
        assignees = [a["login"] for a in current.get("assignees", [])]
        if item["assignee"] not in assignees:
            payload["assignees"] = assignees + [item["assignee"]]
        wanted_labels = {"task", "role:" + item["role"].lower()}
        if item["optional"]:
            wanted_labels.add("optional")
        current_labels = {label["name"] for label in current.get("labels", [])}
        if not wanted_labels.issubset(current_labels):
            payload["labels"] = sorted(current_labels | wanted_labels)
        dependencies = ", ".join(f"{key}: #{numbers[key]}" for key in item["dependencies"]) or "None"
        body = current.get("body") or ""
        new_body = re.sub(r"^- \*\*Dependencies:\*\*.*$", "- **Dependencies:** " + dependencies,
                          body, flags=re.MULTILINE)
        if new_body != body:
            payload["body"] = new_body
        if payload:
            api("PATCH", path, payload)
            updated += 1
    print(f"Verified 5 milestones and 44 mapped issues; updated {updated} issue records.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--validate", action="store_true")
    action.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    plan = json.loads((ROOT / ".github/project-plan.json").read_text())
    validate(plan)
    if args.validate:
        print("Plan valid: 5 milestones, 44 unique issues, valid owners and acyclic dependencies.")
        return
    if os.environ.get("GITHUB_REPOSITORY") != EXPECTED_REPOSITORY:
        raise SystemExit("Apply only from this repository's GitHub Actions workflow.")
    sync(plan)


if __name__ == "__main__":
    main()
