// vim: set ts=2 sts=2 sw=2 et:

async function loadWasm(str, imports = {}) {
  const binary = wasmTextToBinary(str);
  const module = await WebAssembly.compile(binary);
  return new WebAssembly.Instance(module, imports)
}

function wasmExportResultTaintSource() {

  const singleValueTaint = `(module
(func (export "run") (result i32)
(i32.const 10)))`

  loadWasm(singleValueTaint)
    .then(instance => {
      const { run } = instance.exports;
      const result = run();
      assertEq(10, result);
      assertNumberTainted(result);
    });
}

function wasmExportMultiResultTaintSource() {

  const multiValueTaint = `(module
(func (export "run") (result i32 i32 i32)
(i32.const 0)
(i32.const 52)
(i32.const 10)))`

  loadWasm(multiValueTaint)
    .then(instance => {
      const { run } = instance.exports;
      const [a, b, c] = run();
      assertEq(0, a);
      assertEq(52, b);
      assertEq(10, c);
      assertNumberTainted(a);
      assertNumberTainted(b);
      assertNumberTainted(c);
    });


}

function wasmExportMemoryTaintSource() {
  const memoryTaint = `(module
(memory (export "memory") 1)
(func (export "run") (param i32) (param i32) (param i32)
  (memory.fill (local.get 0) (local.get 1) (local.get 2)))
)`

  loadWasm(memoryTaint)
    .then(instance => {
      const { run, memory } = instance.exports;
      //run(0, 42, 10);
      //const m = new Uint8Array(memory.buffer);
      //assertArrayTainted(m);
      //const val = m[0];
      //assertEq(42, val);
      //assertNumberTainted(val);
    });
}

function wasmImportArgTaintSource() {
  const importArgTaint = `(module
(func $timesTwo (import "env" "timesTwo") (param i32) (result i32))
(func (export "run") (result i32)
  (call $timesTwo (i32.const 10)))
)
`
  const imports = {
    env: {
      timesTwo: x => {
        assertNumberTainted(x);
        return x * 2;
      }
    }
  }
  loadWasm(importArgTaint, imports)
    .then(instance => {
      const { run } = instance.exports;
      run();
    });
}

function wasmImportMultiArgTaintSource() {

  const importMultiArgTaint = `(module
(func $multiMulti (import "env" "multiMulti") (param i32 i32) (result i32))
(func (export "run") (result i32)
  (call $multiMulti (i32.const 4) (i32.const 5)))
)`

  const imports = {
    env: {
      multiMulti: (x, y) => {
        assertNumberTainted(x);
        assertNumberTainted(y);
        return x * y;
      }
    }
  }

  loadWasm(importMultiArgTaint, imports)
    .then(instance => {
      const { run } = instance.exports;
      run();
    })
}

function wasmTableResultTaintSource() {

  const importMultiArgTaint = `(module
(func $return42 (result i32) (i32.const 42))
(table $table 1 anyfunc)
(elem (i32.const 0) $return42)
(export "table" (table $table))
)`;

  loadWasm(importMultiArgTaint)
    .then(instance => {
      const { table } = instance.exports;
      const result = table.get(0)();
      assertEq(42, result);
      assertNumberTainted(result);
    })
}

function wasmDecodeStringTaintSource() {

  const decodeStringTaint = `(module
  (memory $mem 1)
  (export "memory" (memory $mem))
  (data (i32.const 0) "\\77\\65\\6D\\62\\79")
  (func $run (result i32)
    (i32.const 0)
  )
  (export "run" (func $run))
)`;

  loadWasm(decodeStringTaint)
    .then(instance => {
      const { run, memory } = instance.exports;
      const result = run();
      assertTainted(result);
      const mem = new Uint8Array(memory.buffer);
      assertArrayTainted(mem);

      let str = "";
      let i = result;
      while (mem[i] != 0) {
        str += String.fromCharCode(mem[i]);
        i++;
      }
      assertTainted(str);
    })
}

runTaintTest(wasmExportResultTaintSource);
runTaintTest(wasmExportMultiResultTaintSource);
runTaintTest(wasmExportMemoryTaintSource);
runTaintTest(wasmImportArgTaintSource);
runTaintTest(wasmImportMultiArgTaintSource);
runTaintTest(wasmTableResultTaintSource);
runTaintTest(wasmDecodeStringTaintSource);

if (typeof reportCompare === 'function')
  reportCompare(true, true);
