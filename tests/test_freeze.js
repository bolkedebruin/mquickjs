"use strict";

function assert(actual, expected, message) {
    if (arguments.length == 1)
        expected = true;
    if (actual !== expected) {
        throw Error("assertion failed: got |" + actual + "|, expected |" + expected + "|" +
                    (message ? " (" + message + ")" : ""));
    }
}

function test_freeze() {
    var obj = { a: 1, b: "hello", c: true };
    Object.freeze(obj);

    // Cannot modify existing properties
    var threw = false;
    try { obj.a = 2; } catch(e1) { threw = true; }
    assert(threw, true, "should throw on write to frozen property");
    assert(obj.a, 1, "value should be unchanged");

    // Cannot add new properties
    threw = false;
    try { obj.d = 4; } catch(e2) { threw = true; }
    assert(threw, true, "should throw on adding property to frozen object");
    assert(obj.d, undefined);

    // Cannot delete properties
    threw = false;
    try { delete obj.a; } catch(e3) { threw = true; }
    assert(threw, true, "should throw on deleting frozen property");
    assert(obj.a, 1);

    // Reading still works
    assert(obj.a, 1);
    assert(obj.b, "hello");
    assert(obj.c, true);

    // Object.isFrozen
    assert(Object.isFrozen(obj), true);
    assert(Object.isFrozen({}), false);
    assert(Object.isFrozen(42), true, "non-objects are trivially frozen");

    // Freeze returns the same object
    var obj2 = { x: 1 };
    var ret = Object.freeze(obj2);
    assert(ret === obj2, true);

    // Freeze on non-object is a no-op, returns the value
    assert(Object.freeze(42), 42);
    assert(Object.freeze(null), null);
    assert(Object.freeze(undefined), undefined);
}

function test_freeze_nested() {
    // Nested objects are NOT deep-frozen (standard behavior)
    var outer = { inner: { val: 1 } };
    Object.freeze(outer);
    var threw = false;
    try { outer.inner = {}; } catch(e1) { threw = true; }
    assert(threw, true, "cannot replace inner on frozen outer");
    // But inner itself is still mutable
    outer.inner.val = 999;
    assert(outer.inner.val, 999, "inner object is not frozen");

    // Deep freeze pattern
    var deep = { nested: { value: 42 } };
    Object.freeze(deep.nested);
    Object.freeze(deep);
    threw = false;
    try { deep.nested.value = 0; } catch(e2) { threw = true; }
    assert(threw, true, "deep-frozen nested property should throw");
}

function test_seal() {
    var sealed = { a: 1, b: 2 };
    Object.seal(sealed);

    // Can modify existing properties
    sealed.a = 10;
    assert(sealed.a, 10, "sealed allows writes to existing props");

    // Cannot add properties
    var threw = false;
    try { sealed.c = 3; } catch(e1) { threw = true; }
    assert(threw, true, "sealed object rejects new properties");

    // Cannot delete properties
    threw = false;
    try { delete sealed.a; } catch(e2) { threw = true; }
    assert(threw, true, "sealed object rejects deletes");

    assert(Object.isSealed(sealed), true);
    assert(Object.isFrozen(sealed), false, "sealed but writable is not frozen");

    // Seal returns the same object
    var sobj = { x: 1 };
    assert(Object.seal(sobj) === sobj, true);
}

function test_preventExtensions() {
    var noext = { a: 1 };
    Object.preventExtensions(noext);

    // Can modify
    noext.a = 2;
    assert(noext.a, 2);

    // Can delete
    delete noext.a;
    assert(noext.a, undefined);

    // Cannot add
    var threw = false;
    try { noext.b = 1; } catch(e1) { threw = true; }
    assert(threw, true, "non-extensible object rejects additions");

    assert(Object.isExtensible(noext), false);
    assert(Object.isExtensible({}), true);

    // preventExtensions returns the same object
    var pobj = { x: 1 };
    assert(Object.preventExtensions(pobj) === pobj, true);

    // preventExtensions on non-object returns the value
    assert(Object.preventExtensions(42), 42);
}

function test_edge_cases() {
    // Freeze empty object
    var empty = {};
    Object.freeze(empty);
    assert(Object.isFrozen(empty), true);
    var threw = false;
    try { empty.x = 1; } catch(e1) { threw = true; }
    assert(threw, true);

    // Freeze object with many properties
    var big = {};
    for (var i = 0; i < 50; i++) {
        big["prop" + i] = i;
    }
    Object.freeze(big);
    assert(big.prop0, 0);
    assert(big.prop49, 49);
    threw = false;
    try { big.prop0 = -1; } catch(e2) { threw = true; }
    assert(threw, true);
    assert(big.prop0, 0);

    // isExtensible on non-objects
    assert(Object.isExtensible(42), false);
    assert(Object.isExtensible(null), false);
    assert(Object.isExtensible(undefined), false);
    assert(Object.isExtensible(true), false);

    // isSealed on non-objects
    assert(Object.isSealed(42), true);
    assert(Object.isSealed(null), true);

    // isFrozen on non-objects
    assert(Object.isFrozen(42), true);
    assert(Object.isFrozen(null), true);
    assert(Object.isFrozen(true), true);
}

function test_freeze_keys() {
    // Freeze object then check keys still work
    var fkeys = { a: 1, b: 2, c: 3 };
    Object.freeze(fkeys);
    var keys = Object.keys(fkeys);
    assert(keys.length, 3);

    // for-in works on frozen objects
    var count = 0;
    for (var k in fkeys) {
        count++;
    }
    assert(count, 3);
}

function test_seal_then_freeze() {
    // isFrozen on sealed object where all props are non-writable
    var sealfrozen = { x: 1 };
    Object.freeze(sealfrozen);
    assert(Object.isFrozen(sealfrozen), true);
    assert(Object.isSealed(sealfrozen), true);

    // Seal then freeze
    var sf = { a: 1, b: 2 };
    Object.seal(sf);
    assert(Object.isSealed(sf), true);
    assert(Object.isFrozen(sf), false);
    Object.freeze(sf);
    assert(Object.isFrozen(sf), true);
}

test_freeze();
test_freeze_nested();
test_seal();
test_preventExtensions();
test_edge_cases();
test_freeze_keys();
test_seal_then_freeze();
print("test_freeze: all tests passed");
