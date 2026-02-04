"use strict";

var assert_count = 0;

function assert(actual, expected, message) {
    assert_count++;
    if (actual !== expected) {
        throw new Error("FAIL #" + assert_count + ": " + message +
                        " (expected " + expected + ", got " + actual + ")");
    }
}

/* Test 1: Flat object with various primitive types */
// @rom
const config1 = { x: 42, name: "hello", flag: true, empty: null, off: false };

assert(config1.x, 42, "flat int");
assert(config1.name, "hello", "flat string");
assert(config1.flag, true, "flat bool true");
assert(config1.empty, null, "flat null");
assert(config1.off, false, "flat bool false");

/* Test 2: Object.isFrozen / isSealed / isExtensible on @rom */
assert(Object.isFrozen(config1), true, "isFrozen");
assert(Object.isSealed(config1), true, "isSealed");
assert(Object.isExtensible(config1), false, "isExtensible");

/* Test 3: Frozen behavior - cannot modify (without closures) */
var threw = false;
try { config1.x = 99; } catch(e1) { threw = true; }
assert(threw, true, "write to frozen property throws");
assert(config1.x, 42, "value unchanged after write attempt");

threw = false;
try { config1.newProp = 1; } catch(e2) { threw = true; }
assert(threw, true, "add property to frozen throws");

/* Test 4: Multiple @rom consts — verify independence */
// @rom
const obj_a = { val: 1 };
// @rom
const obj_b = { val: 2 };

assert(obj_a.val, 1, "independence a");
assert(obj_b.val, 2, "independence b");

/* Test 5: Empty @rom object */
// @rom
const empty_obj = {};

assert(Object.isFrozen(empty_obj), true, "empty frozen");
assert(Object.isExtensible(empty_obj), false, "empty not extensible");

/* Test 6: Number edge cases */
// @rom
const nums = { zero: 0, neg: -5, large: 12345, pi: 3.14 };

assert(nums.zero, 0, "zero");
assert(nums.neg, -5, "negative");
assert(nums.large, 12345, "large int");
assert(nums.pi, 3.14, "float");

/* Test 7: Property enumeration */
// @rom
const enum_obj = { a: 1, b: 2, c: 3 };

var keys = Object.keys(enum_obj);
assert(keys.length, 3, "keys length");
assert(keys[0], "a", "key 0");
assert(keys[1], "b", "key 1");
assert(keys[2], "c", "key 2");

/* Test 8: for-in on @rom */
var for_in_keys = [];
for (var k in enum_obj) {
    for_in_keys.push(k);
}
assert(for_in_keys.length, 3, "for-in length");

/* Test 9: Nested objects — both levels frozen */
// @rom
const nested = { colors: { bg: 0, fg: 255 }, version: 1 };

assert(nested.version, 1, "nested parent prop");
assert(nested.colors.bg, 0, "nested child bg");
assert(nested.colors.fg, 255, "nested child fg");
assert(Object.isFrozen(nested), true, "nested parent frozen");
assert(Object.isFrozen(nested.colors), true, "nested child frozen");
assert(Object.isExtensible(nested), false, "nested parent not extensible");
assert(Object.isExtensible(nested.colors), false, "nested child not extensible");

/* Test 10: Array values — array frozen */
// @rom
const with_arr = { items: [10, 20, 30] };

assert(with_arr.items.length, 3, "array length");
assert(with_arr.items[0], 10, "array[0]");
assert(with_arr.items[1], 20, "array[1]");
assert(with_arr.items[2], 30, "array[2]");
assert(Object.isFrozen(with_arr), true, "parent with array frozen");
assert(Object.isFrozen(with_arr.items), true, "array frozen");

/* Test 11: @rom as function argument (pass via global var) */
// @rom
const arg_obj = { x: 99 };
var arg_obj_copy = arg_obj;
function read_val(obj) {
    return obj.x;
}
assert(read_val(arg_obj_copy), 99, "function argument");

/* Test 12: Copy @rom data to mutable object */
// @rom
const src = { a: 1, b: 2 };
var dst = {};
var src_keys = Object.keys(src);
for (var i = 0; i < src_keys.length; i++) {
    dst[src_keys[i]] = src[src_keys[i]];
}
dst.a = 100;
assert(dst.a, 100, "mutable copy modified");
assert(src.a, 1, "original unchanged");

/* Test 13: Deeply nested */
// @rom
const deep = { level1: { level2: { val: 42 } } };
assert(deep.level1.level2.val, 42, "deep nested access");
assert(Object.isFrozen(deep), true, "deep parent frozen");
assert(Object.isFrozen(deep.level1), true, "deep level1 frozen");
assert(Object.isFrozen(deep.level1.level2), true, "deep level2 frozen");

/* Test 14: Mixed nested and flat in same object */
// @rom
const mixed = { a: 1, inner: { b: 2 }, c: 3 };
assert(mixed.a, 1, "mixed flat a");
assert(mixed.inner.b, 2, "mixed nested b");
assert(mixed.c, 3, "mixed flat c");
assert(Object.isFrozen(mixed), true, "mixed frozen");
assert(Object.isFrozen(mixed.inner), true, "mixed inner frozen");

/* Test 15: String array */
// @rom
const str_arr = { names: ["alice", "bob"] };
assert(str_arr.names[0], "alice", "string array 0");
assert(str_arr.names[1], "bob", "string array 1");

/* Test 16: Frozen write in non-closure context */
// @rom
const frozen_write_test = { val: 10 };
threw = false;
try { frozen_write_test.val = 20; } catch(e3) { threw = true; }
assert(threw, true, "frozen write throws");
assert(frozen_write_test.val, 10, "frozen value unchanged");

/* Test 17: Delete on frozen object */
// @rom
const del_test = { x: 1 };
threw = false;
try { delete del_test.x; } catch(e4) { threw = true; }
assert(threw, true, "delete on frozen throws");

print("All " + assert_count + " @rom pragma tests passed.");
