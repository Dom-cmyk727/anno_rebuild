#!/usr/bin/env python3
"""Independent ECON-BOOK-1 checked model; never the original-wrap oracle.

Integer formulas and chronological events from docs/sim/economy-booking.md.
No C++/assets/expected answers are imported. The CLI executes every complete
JSON case and compares its entire result. All failed calculations are immutable.
Python's unbounded integers deliberately make the supported widths explicit.
"""
import argparse
import copy
import json
from pathlib import Path
import sys

U16 = (1 << 16) - 1
U32 = (1 << 32) - 1
U64 = (1 << 64) - 1
I64_MIN = -(1 << 63)
I64_MAX = (1 << 63) - 1


class Invalid(ValueError):
    pass


def bounded(value, low, high, name):
    if type(value) is not int or not low <= value <= high:
        raise Invalid(name)
    return value


def unsigned(value, high, name):
    return bounded(value, 0, high, name)


def book(*, tick, stamp, rows, money=0, costs=(0, 0), stats=(0, 0, 0), period=2000):
    """One city visit. A fault carries no partial money/stamp/counter mutation."""
    unsigned(tick, U32, 'tick')
    unsigned(stamp, U16, 'stamp')
    bounded(period, 1, U16, 'period')
    bounded(money, I64_MIN, I64_MAX, 'money')
    if len(costs) != 2 or len(stats) != 3:
        raise Invalid('dimensions')
    for value in costs:
        unsigned(value, U64, 'cost')
    for value in stats:
        unsigned(value, U64, 'stat')
    bills = []
    for row in rows:
        if len(row) != 3:
            raise Invalid('row')
        for name, value in zip(('price', 'surcharge', 'bill'), row):
            unsigned(value, U32, name)
        bills.append(row[2])
    unchanged = dict(stepped=False, stamp=stamp, bills=bills, money=money,
                     stats=list(stats), income=0, upkeep=0)
    if (tick - stamp) % (U16 + 1) < period:
        return unchanged
    try:
        total = 0
        for price, surcharge, bill in rows:
            product = unsigned((price + surcharge) * bill, U64, 'product')
            total = unsigned(total + product, U64, 'sum')
        income = unsigned(total + 128, U64, 'rounding') // 256
        upkeep = unsigned(sum(costs), U64, 'cost_sum') // 15
        credited = bounded(money + income, I64_MIN, I64_MAX, 'credit')
        balance = bounded(credited - upkeep, I64_MIN, I64_MAX, 'debit')
        decayed = [n * 104 // 105 for n in stats]
        decayed[0] = unsigned(decayed[0] + income, U64, 'sales')
    except Invalid as exc:
        return dict(unchanged, fault=str(exc))
    return dict(stepped=True, stamp=tick % (U16 + 1), bills=[0] * len(rows),
                money=balance, stats=decayed, income=income, upkeep=upkeep)


def prices(*, climates, tax_count, directives):
    """Ordered source directives: Preis resets prices only; partial tax overwrites."""
    if not climates or len(set(climates)) != len(climates) or any(
            type(c) is not str or not c for c in climates):
        raise Invalid('climates')
    bounded(tax_count, 1, U32, 'tax_count')
    values, tax = [0] * len(climates), [0] * tax_count
    for directive in directives:
        if type(directive) is not list or not directive:
            raise Invalid('directive')
        kind, *args = directive
        if kind == 'Preis' and len(args) == 1:
            price = unsigned(args[0], U32, 'Preis')
            values = [price] * len(climates)
        elif kind == 'PreisLevel' and len(args) == 1 and type(args[0]) is list:
            if len(args[0]) > tax_count:
                raise Invalid('PreisLevel.columns')
            for i, value in enumerate(args[0]):
                tax[i] = unsigned(value, U32, 'PreisLevel')
        elif kind in ('Bonus', 'Malus') and len(args) == 2:
            if args[0] not in climates:
                raise Invalid('climate')
            delta = unsigned(args[1], U32, kind) * (1 if kind == 'Bonus' else -1)
            i = climates.index(args[0])
            values[i] = bounded(values[i] + delta, I64_MIN, I64_MAX, 'price_accumulator')
        else:
            raise Invalid('directive')
    for value in values:
        unsigned(value, U32, 'resolved_price')
    return dict(prices=values, tax=tax)


def bill(*, kind, request, stock, pending):
    unsigned(request, U32, 'request')
    unsigned(stock, U32, 'stock')
    unsigned(pending, U32, 'pending')
    if kind not in ('stall', 'service', 'inn', 'merge', 'return'):
        raise Invalid('billing_kind')
    delivered = min(request, stock) if kind in ('stall', 'inn') else 0
    amount = delivered if kind == 'inn' else (0 if kind == 'return' else request)
    if kind == 'merge':
        added = min(amount, U32 - pending)
    elif pending + amount > U32:
        return dict(pending=pending, stock=stock, delivered=0, added=0, fault='bill')
    else:
        added = amount
    return dict(pending=pending + added, stock=stock - delivered,
                delivered=delivered, added=added)


def clock(*, elapsed, initial_ms=0):
    unsigned(initial_ms, U64, 'initial_ms')
    now = initial_ms
    visits = []
    for ms in elapsed:
        unsigned(ms, U32, 'elapsed')
        now = unsigned(now + ms, U64, 'clock')
        visits.append(dict(ms=now, tick=(now // 2) % (U32 + 1), surplus=now % 2))
    return dict(visits=visits)


def sequence(*, cities, money, events):
    """Chronological semantic model, independent of engine effect-prefix storage.

    Generic effects retain accepted saturation even after a fault. A fault bars
    later bookings, but does not erase an already committed earlier booking.
    """
    cities, money = copy.deepcopy(cities), copy.deepcopy(money)
    stopped, bookings = False, []
    for event in events:
        kind = event['kind']
        if kind == 'money':
            owner = event['owner']
            bounded(event['delta'], I64_MIN, I64_MAX, 'delta')
            money[owner] = min(I64_MAX, max(I64_MIN, money[owner] + event['delta']))
        elif kind == 'bill':
            city = cities[event['city']]
            unsigned(event['delta'], U32, 'delta')
            row = city['rows'][event['good']]
            row[2] = min(U32, row[2] + event['delta'])
        elif kind == 'fault':
            stopped = True
        elif kind == 'book':
            if stopped:
                bookings.append(dict(city=event['city'], suppressed=True))
                continue
            city = cities[event['city']]
            args = {k: v for k, v in city.items() if k != 'owner'}
            result = book(**args, tick=event['tick'], money=money[city['owner']])
            bookings.append(dict(city=event['city'], **result))
            if 'fault' in result:
                stopped = True
            elif result['stepped']:
                money[city['owner']] = result['money']
                city['stamp'], city['stats'] = result['stamp'], result['stats']
                for row in city['rows']:
                    row[2] = 0
        else:
            raise Invalid('event')
    return dict(cities=cities, money=money, stopped=stopped, bookings=bookings)


def lifecycle(*, events):
    """History, not a sum reconstructed from current types.

    Houses retain their original contribution on type changes. Weapons research
    changes the selector, without a notification that adjusts historical costs.
    Failed and duplicate operations leave the entire financial model unchanged.
    """
    actors, costs, dispositions = {}, [0, 0], []

    def active(actor):
        if actor['class'] == 'weapons':
            return any(on and eligible for on, eligible in zip(actor['on'], actor['eligible']))
        if actor['class'] == 'dual':
            return any(actor['on'])
        return actor['on'][0]

    def contribution(actor):
        return actor['cost'][int(active(actor))]

    for event in events:
        before_actors, before_costs = copy.deepcopy(actors), costs[:]
        try:
            key, op = event['id'], event['op']
            if op == 'create':
                if key in actors:
                    raise Invalid('duplicate')
                actor = copy.deepcopy(event['actor'])
                if actor['kind'] not in (1, 2) or len(actor['cost']) != 2:
                    raise Invalid('actor')
                if actor['class'] not in ('single', 'dual', 'weapons', 'house', 'inert'):
                    raise Invalid('class')
                lines = 2 if actor['class'] in ('dual', 'weapons') else 1
                if len(actor['on']) != lines or any(type(on) is not bool for on in actor['on']):
                    raise Invalid('switches')
                if actor['class'] == 'weapons' and (len(actor['eligible']) != 2 or any(
                        type(eligible) is not bool for eligible in actor['eligible'])):
                    raise Invalid('research')
                for value in actor['cost']:
                    unsigned(value, U32, 'running_cost')
                actors[key] = actor
                bucket = actor['kind'] - 1
                costs[bucket] = unsigned(costs[bucket] + contribution(actor), U64, 'cost_add')
            else:
                if key not in actors:
                    raise Invalid('missing')
                actor = actors[key]
                bucket = actor['kind'] - 1
                old = contribution(actor)
                if op == 'switch':
                    if actor['class'] not in ('single', 'dual', 'weapons'):
                        raise Invalid('unsupported')
                    actor['on'][event['line']] = event['on']
                    new = contribution(actor)
                    if old != new:
                        costs[bucket] = unsigned(costs[bucket] - old, U64, 'cost_sub')
                        costs[bucket] = unsigned(costs[bucket] + new, U64, 'cost_add')
                elif op == 'research':
                    actor['eligible'][event['line']] = event['eligible']
                elif op == 'house_replace':
                    if actor['class'] != 'house':
                        raise Invalid('unsupported')
                    for value in event['cost']:
                        unsigned(value, U32, 'running_cost')
                    actor['cost'] = event['cost'][:]
                elif op == 'remove':
                    costs[bucket] = unsigned(costs[bucket] - old, U64, 'cost_sub')
                    del actors[key]
                else:
                    raise Invalid('unsupported')
            dispositions.append('ok')
        except Invalid as exc:
            actors, costs = before_actors, before_costs
            dispositions.append(str(exc))
    return dict(actors=actors, costs=costs, dispositions=dispositions)


def city_player(*, city_cost, player_a, player_b, remainder, visits):
    unsigned(city_cost, U64, 'cost')
    for value in (player_a, player_b, remainder):
        unsigned(value, U32, 'player')
    bounded(visits, 1, U32, 'visits')
    city_charge, player_charge = 0, 0
    for _ in range(visits):
        city_charge += city_cost // 15
        total = (player_a + player_b + remainder) % (U32 + 1)
        player_charge += total // 15
        remainder = total % 15
    return dict(city_charge=city_charge, player_charge=player_charge, remainder=remainder)


MODELS = dict(book=book, prices=prices, bill=bill, clock=clock,
              sequence=sequence, lifecycle=lifecycle, city_player=city_player)


def evaluate(case):
    if set(case) != {'id', 'kind', 'input', 'expected'} or not case['id'] or type(case['id']) is not str:
        raise Invalid('case fields')
    if case['kind'] not in MODELS or type(case['input']) is not dict:
        raise Invalid('case kind/input')
    if type(case['expected']) is not dict or not case['expected']:
        raise Invalid('empty expected')
    try:
        return MODELS[case['kind']](**case['input'])
    except Invalid as exc:
        return dict(invalid=str(exc))


def run(path):
    try:
        document = json.loads(Path(path).read_text())
        if type(document) is not dict or set(document) != {'version', 'cases'} or document['version'] != 1:
            raise Invalid('document')
        cases = document['cases']
        if type(cases) is not list or not cases:
            raise Invalid('empty cases')
        ids = [c['id'] for c in cases]
        if len(set(ids)) != len(ids):
            raise Invalid('duplicate id')
        results = []
        for case in cases:
            actual = evaluate(case)
            results.append(dict(id=case['id'], actual=actual))
            if actual != case['expected']:
                print(json.dumps(dict(id=case['id'], expected=case['expected'], actual=actual)), file=sys.stderr)
                return 1
        print(json.dumps(dict(executed=len(results), results=results), sort_keys=True))
        return 0
    except (OSError, ValueError, KeyError, TypeError, IndexError, OverflowError) as exc:
        print('invalid reference input: ' + str(exc), file=sys.stderr)
        return 2


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('vectors')
    sys.exit(run(parser.parse_args().vectors))
