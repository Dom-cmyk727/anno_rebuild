"""Independent MAP-ROUTE-1 contract model; original evidence: docs/re/ship-trade-routes.md.

Research/contract reference, not a game controller. No original assets are read.
Inputs are synthetic quantities/actions/notifications; expected answers are used
only by the checker, never by the computation. Run check/selftest <vectors.jsonl>.
"""
import copy
import json
import struct
import sys

U64 = (1 << 64) - 1
TON = 256


def integer(value, lo=0, hi=U64):
    if type(value) is not int or not lo <= value <= hi:
        raise ValueError("integer outside declared domain")
    return value


def exact(obj, keys):
    if type(obj) is not dict or set(obj) != set(keys):
        raise ValueError("wrong object fields")


def normalize(actions):
    if type(actions) is not list:
        raise ValueError("actions must be a list")
    result, first = [], {}
    for item in actions:
        exact(item, ("direction", "goods", "tons"))
        direction = integer(item["direction"], 0, 1)
        goods = integer(item["goods"], 1, (1 << 32) - 1)
        tons = integer(item["tons"], 0, U64 // TON)
        key = direction, goods
        result.append(dict(direction=direction, goods=goods, tons=0))
        if key not in first:
            first[key] = len(result) - 1
        at = first[key]
        result[at]["tons"] += tons
        if result[at]["tons"] > U64 // TON:
            raise ValueError("group quantity overflow")
    return result


def load_hold(holds, goods, capacity, preferred):
    room = lambda h: max(0, capacity - h[1])
    if holds[preferred][0] == goods and room(holds[preferred]) >= TON:
        return preferred
    chosen, best = None, 0
    for i, h in enumerate(holds):
        if h == [0, 0]:
            if chosen is None:
                chosen = i
        elif h[0] == goods and room(h) > best:
            chosen, best = i, room(h)
    return chosen


def cargo(inp):
    exact(inp, ("actions", "holds", "hold_capacity", "stock", "store_capacity"))
    actions = normalize(inp["actions"])
    capacity = integer(inp["hold_capacity"], 1, (1 << 32) - 1)
    store_capacity = integer(inp["store_capacity"], 0, (1 << 32) - 1)
    if type(inp["holds"]) is not list or type(inp["stock"]) is not dict:
        raise ValueError("holds/stock shape")
    holds = []
    for hold in inp["holds"]:
        if type(hold) is not list or len(hold) != 2:
            raise ValueError("hold shape")
        holds.append([integer(hold[0], 0, (1 << 32) - 1), integer(hold[1])])
    stock = {}
    for key, value in inp["stock"].items():
        if not key.isdecimal() or str(int(key)) != key:
            raise ValueError("stock goods key")
        stock[key] = integer(value, 0, (1 << 32) - 1)
    effects = []
    for ai, action in enumerate(actions):
        g, direction = action["goods"], action["direction"]
        key = str(g)
        if key not in stock:
            raise ValueError("unsupported fixture goods")
        matching = [i for i, h in enumerate(holds) if h[0] == g]
        aboard = sum(holds[i][1] for i in matching)
        if aboard > U64:
            raise ValueError("aboard overflow")
        requested = action["tons"] * TON
        remaining = max(0, requested - aboard) if direction == 0 else requested
        candidates = ([i for i in matching if holds[i][1] < capacity] +
                      [i for i, h in enumerate(holds) if h == [0, 0]]) if direction == 0 else matching
        for selected in candidates:
            if remaining == 0:
                break
            bound = max(0, capacity - holds[selected][1]) if direction == 0 else holds[selected][1]
            tons = min(remaining, bound) // TON
            if tons == 0:
                effects.append([ai, selected, 0, 0, "whole_ton_zero"])
                break
            actual_hold = load_hold(holds, g, capacity, selected) if direction == 0 else selected
            if direction == 0:
                available = stock[key] // TON * TON
                if available == 0 or actual_hold is None:
                    effects.append([ai, selected, tons, 0, "resource_zero"])
                    break
                moved = min(tons * TON, available, max(0, capacity - holds[actual_hold][1]))
                stock[key] -= moved
                holds[actual_hold] = [g, holds[actual_hold][1] + moved]
            else:
                moved = min(tons * TON, holds[selected][1], max(0, store_capacity - stock[key]))
                stock[key] += moved
                holds[selected][1] -= moved
                if holds[selected][1] == 0:
                    holds[selected][0] = 0
            effects.append([ai, actual_hold, tons, moved, "transfer" if moved else "resource_zero"])
            remaining -= moved
            if moved == 0:
                break
    return {"actions": actions, "holds": holds, "stock": stock,
            "effects": effects, "waits": len(actions)}


def delays(inp):
    """Isolated integer wait/action stages; no physical movement-time oracle."""
    exact(inp, ("waits", "visits", "move_after"))
    waits = [integer(n, 0, (1 << 32) - 1) for n in inp["waits"]]
    if not waits or type(inp["move_after"]) is not bool:
        raise ValueError("nonempty waits and boolean boundary required")
    index, remaining, credit, trace = 0, waits[0], 0, []
    for visit in inp["visits"]:
        exact(visit, ("elapsed", "budget"))
        credit += integer(visit["elapsed"])
        if credit > U64:
            raise ValueError("credit overflow")
        budget = integer(visit["budget"], 1, (1 << 32) - 1)
        while index < len(waits) and budget:
            spent = min(credit, remaining)
            remaining -= spent
            credit -= spent
            if remaining:
                break
            index += 1
            budget -= 1
            remaining = waits[index] if index < len(waits) else 0
        if index == len(waits) and inp["move_after"]:
            credit = 0  # explicit forward-only Move issuance boundary, not backdated sailing
        trace.append([index, remaining, credit])
    return {"trace": trace}


def progress(inp):
    """Generation/order authority and cursor lifecycle, independent of pathfinding."""
    exact(inp, ("stops", "events"))
    stops = integer(inp["stops"], 1, (1 << 32) - 1)
    generation, next_order = 0, 0
    state = None
    trace = []
    for event in inp["events"]:
        if type(event) is not list or not event:
            raise ValueError("event shape")
        op = event[0]
        if op == "assign" and len(event) == 1:
            generation += 1
            next_order += 1
            state = [0, "sailing", next_order, 0]
        elif op == "arrive" and len(event) == 3:
            rev, order = integer(event[1]), integer(event[2])
            if state and state[1] == "sailing" and rev == generation and order == state[2]:
                state[1], state[2] = "wait", 0
        elif op == "complete" and len(event) == 1:
            if state and state[1] == "wait":
                state[0] += 1
                if state[0] == stops:
                    state[0] = 0
                    state[3] += 1
                next_order += 1
                state[1], state[2] = "sailing", next_order
        elif op == "suspend" and len(event) == 1:
            if state and not state[1].startswith("paused-"):
                state[1] = "paused-" + state[1]
                state[2] = 0
        elif op == "resume" and len(event) == 1:
            if state and state[1].startswith("paused-"):
                state[1] = state[1][7:]
                if state[1] == "sailing":
                    next_order += 1
                    state[2] = next_order
        elif op in ("cancel", "manual") and len(event) == 1:
            state = None
        else:
            raise ValueError("unknown or malformed event")
        trace.append({"generation": generation, "state": copy.deepcopy(state)})
    return {"trace": trace}


def wire(inp):
    exact(inp, ("ship", "control"))
    ship = integer(inp["ship"])
    control = integer(inp["control"], 0, 2)
    return {"hex": struct.pack("<HHQB", 0xF076, 9, ship, control).hex()}


def assign_wire(inp):
    exact(inp, ("ship", "stops"))
    if type(inp["stops"]) is not list or not inp["stops"]:
        raise ValueError("nonempty stop list required")
    payload = bytearray(struct.pack("<QI", integer(inp["ship"]), len(inp["stops"])))
    for stop in inp["stops"]:
        exact(stop, ("warehouse", "dock_x", "dock_y", "actions"))
        normalize(stop["actions"])  # validate without replacing recorded input quantities
        payload += struct.pack("<QiiI", integer(stop["warehouse"]),
                               integer(stop["dock_x"], -(1 << 31), (1 << 31) - 1),
                               integer(stop["dock_y"], -(1 << 31), (1 << 31) - 1), len(stop["actions"]))
        for action in stop["actions"]:
            payload += struct.pack("<BIQ", action["direction"], action["goods"], action["tons"])
    if len(payload) > 65535:
        raise ValueError("ClientRecord payload overflow")
    return {"hex": (struct.pack("<HH", 0xF075, len(payload)) + payload).hex()}


def native(inp):
    """Independent new Figure suffix, including a nonempty route progress record."""
    exact(inp, ("generation", "state"))
    out = bytearray(struct.pack("<Q", integer(inp["generation"])))
    state = inp["state"]
    out.append(int(state is not None))
    if state is None:
        return {"hex": out.hex()}
    exact(state, ("stops", "status", "phase", "stop_index", "action_index", "expected_order_id",
                  "order_generation", "last_tick", "credit_ticks", "wait_remaining", "scan_next",
                  "matching_raw", "remaining_raw", "matching_holds", "empty_holds",
                  "candidate_next", "completed_cycles", "last_reject", "nav_failure"))
    out += struct.pack("<Q", len(state["stops"]))
    for stop in state["stops"]:
        exact(stop, ("warehouse", "dock_x", "dock_y", "actions"))
        out += struct.pack("<QiiQ", integer(stop["warehouse"]),
                           integer(stop["dock_x"], -(1 << 31), (1 << 31) - 1),
                           integer(stop["dock_y"], -(1 << 31), (1 << 31) - 1), len(stop["actions"]))
        for action in stop["actions"]:
            exact(action, ("direction", "goods", "tons"))
            out += struct.pack("<BIQ", integer(action["direction"], 0, 1),
                               integer(action["goods"], 1, (1 << 32) - 1),
                               integer(action["tons"], 0, U64 // TON))
    for key, fmt, hi in [("status", "B", 2), ("phase", "B", 5),
                         ("stop_index", "I", (1 << 32) - 1),
                         ("action_index", "I", (1 << 32) - 1),
                         ("expected_order_id", "Q", U64), ("order_generation", "Q", U64),
                         ("last_tick", "Q", U64), ("credit_ticks", "Q", U64),
                         ("wait_remaining", "I", (1 << 32) - 1),
                         ("scan_next", "I", (1 << 32) - 1),
                         ("matching_raw", "Q", U64), ("remaining_raw", "Q", U64)]:
        out += struct.pack("<" + fmt, integer(state[key], 0, hi))
    for key in ("matching_holds", "empty_holds"):
        out += struct.pack("<Q", len(state[key]))
        for index in state[key]:
            out += struct.pack("<I", integer(index, 0, (1 << 32) - 1))
    out += struct.pack("<IQHB", integer(state["candidate_next"], 0, (1 << 32) - 1),
                       integer(state["completed_cycles"]), integer(state["last_reject"], 0, 65535),
                       integer(state["nav_failure"], 0, 255))
    return {"hex": out.hex()}


def evaluate(case):
    exact(case, ("name", "kind", "input", "expect"))
    if type(case["name"]) is not str or not case["name"]:
        raise ValueError("nonempty name required")
    if case["kind"] not in ("cargo", "delays", "progress", "wire", "assign_wire", "native"):
        raise ValueError("unknown vector kind")
    return {"cargo": cargo, "delays": delays, "progress": progress, "wire": wire,
            "assign_wire": assign_wire, "native": native}[case["kind"]](case["input"])


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON key")
        result[key] = value
    return result


def read_cases(path):
    with open(path, encoding="utf-8") as stream:
        lines = [line for line in stream if line.strip()]
    if not lines:
        raise ValueError("zero executed vectors")
    cases = [json.loads(line, object_pairs_hook=unique_object) for line in lines]
    names = [case.get("name") for case in cases if type(case) is dict]
    if len(names) != len(cases) or len(set(names)) != len(names):
        raise ValueError("duplicate/malformed names")
    return cases


def main(argv):
    if len(argv) != 3 or argv[1] not in ("check", "selftest"):
        raise ValueError("usage: freight_ref.py check|selftest vectors.jsonl")
    cases = read_cases(argv[2])
    for case in cases:
        got = evaluate(case)
        if got != case["expect"]:
            raise ValueError(f"{case['name']}: mismatch: {got!r} != {case['expect']!r}")
    if argv[1] == "selftest":
        # Computation must reject an intentionally wrong expectation AFTER a legal computation.
        for case in cases:
            altered = copy.deepcopy(case)
            altered["expect"] = {"wrong": True}
            if evaluate(altered) == altered["expect"]:
                raise ValueError("expectation-echo mutant escaped")
        if delays({"waits": [500, 500], "visits": [{"elapsed": 1250, "budget": 1},
                                                   {"elapsed": 0, "budget": 1}],
                   "move_after": False}) != {"trace": [[1, 500, 750], [2, 0, 250]]}:
            raise ValueError("surplus/quota control")
    print(f"freight reference: executed {len(cases)} vectors; {argv[1]} passed")


if __name__ == "__main__":
    try:
        main(sys.argv)
    except (OSError, ValueError, TypeError, KeyError, IndexError, OverflowError, struct.error) as error:
        print(f"freight reference: {error}", file=sys.stderr)
        sys.exit(1)
