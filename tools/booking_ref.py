#!/usr/bin/env python3
"""Research oracle for AnnoWelt 0x10029950..0x10029aa4, NOT engine policy.

Explicit x86 word/dword arithmetic, including overflow. Rows are already selected
consumer goods with resolved climate/tax columns: (price_u16, surcharge_u16,
billed_u16). Resolving those columns, upkeep sources and scheduler order are
separate research tasks. Never use this oracle's wrapping as an implicit modern
engine policy. No game assets or C++ implementation are imported.
"""
from dataclasses import dataclass


def u32(n):
    return n & 0xffffffff


def i32(n):
    n = u32(n)
    return n if n < 0x80000000 else n - 0x100000000


def trunc_div(n, d):
    return n // d if n >= 0 else -((-n) // d)


def checked(value, maximum, name):
    if type(value) is not int or not 0 <= value <= maximum:
        raise ValueError(name + ' outside original unsigned field')
    return value


@dataclass(frozen=True)
class Booking:
    stepped: bool
    stamp: int
    billed: tuple
    income: int
    upkeep: int
    money: int
    sales: int
    trade_sales: int
    trade_purchases: int


def book(*, tick, stamp, rows, money=0, civil=0, military=0,
         sales=0, trade_sales=0, trade_purchases=0):
    """One original call; inputs immutable, no catch-up loop or engine mutation."""
    checked(tick, 0xffffffff, 'tick')
    checked(stamp, 0xffff, 'stamp')
    if type(money) is not int or not -(1 << 31) <= money < (1 << 31):
        raise ValueError('money outside original signed dword')
    for name, value in [('civil', civil), ('military', military), ('sales', sales),
                        ('trade_sales', trade_sales), ('trade_purchases', trade_purchases)]:
        checked(value, 0xffffffff, name)
    rows = tuple(tuple(row) for row in rows)
    for row in rows:
        if len(row) != 3:
            raise ValueError('row requires price, surcharge and billed')
        for name, value in zip(('price', 'surcharge', 'billed'), row):
            checked(value, 0xffff, name)
    billed = tuple(row[2] for row in rows)
    if ((tick - stamp) & 0xffff) < 2000:
        return Booking(False, stamp, billed, 0, 0, money, sales, trade_sales, trade_purchases)
    # 0x100299bc..de: two zero-extended words added, low dword of imul,
    # sum in EBX. 0x100299e6..fc: signed truncation of wrapped(sum + 128)/256.
    total = 0
    for price, surcharge, amount in rows:
        total = u32(total + u32((price + surcharge) * amount))
    income = trunc_div(i32(total + 128), 256)
    # mul/shift sequences implement unsigned floor division after dword wraps.
    upkeep = u32(civil + military) // 15
    decay = lambda n: u32(n * 104) // 105
    return Booking(True, tick & 0xffff, (0,) * len(rows), income, upkeep,
                   i32(i32(money + income) - upkeep), u32(decay(sales) + income),
                   decay(trade_sales), decay(trade_purchases))
