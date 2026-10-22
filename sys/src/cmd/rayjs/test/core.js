function assert(v) { if (!v) throw Error('core check failed'); }
assert(NaN !== NaN && !(NaN < 0) && !(NaN >= 0));
assert(Object.is(Math.atanh(-0), -0));
assert(Object.is(Math.trunc(-0.5), -0));
assert(Object.is(Math.min(0, -0), -0));
assert(Object.is(Math.max(-0, 0), 0));
assert(Number.isNaN(Math.sqrt(-1)));
assert(Number.isNaN(Math.log(-1)));
assert(Math.log(Number.MIN_VALUE) < -744);
assert(Math.log2(1024) === 10);
assert(Math.expm1(1e-20) === 1e-20);
assert(Math.log1p(1e-20) === 1e-20);
assert(Math.abs(Math.sin(1e100) + 0.3806377310050287) < 1e-15);
assert(Math.hypot(Infinity, NaN) === Infinity);
assert(2n ** 100n === 1267650600228229401496703205376n);
assert(/\p{Letter}+/u.test('città世界'));
assert('e\u0301'.normalize() === 'é');
assert(Date.now() > 1700000000000 && Date.now() < 3000000000000);
assert(new Date('2000-01-01T00:00:00Z').getUTCFullYear() === 2000);
let caught = false;
try { (function recurse(){recurse()})(); }
catch(e) { caught = e instanceof InternalError && e.message === 'stack overflow'; }
assert(caught);
// The test runner supplies a 4 MiB JavaScript heap limit.
caught = false;
try { new ArrayBuffer(16 * 1024 * 1024); }
catch(e) { caught = e instanceof InternalError && e.message === 'out of memory'; }
assert(caught);
const longname = 'x'.repeat(1024);
try { eval('function ' + longname + '(){ throw Error("long stack"); } ' + longname + '()'); }
catch(e) { assert(e.stack.includes(longname)); }
print('rayjs: core passed');
