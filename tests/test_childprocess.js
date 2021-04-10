import * as os from 'os';
import * as std from 'std';
import inspect from 'inspect.so';
import child_process from 'child-process.so';
import Console from './console.js';
import {   toString } from 'mmap.so';

('use strict');

globalThis.inspect = inspect;

function WriteFile(file, data) {
  let f = std.open(file, 'w+');
  f.puts(data);
  console.log('Wrote "' + file + '": ' + data.length + ' bytes');
}

function DumpChildProcess(cp) {
  const { file, cwd, args, env, stdio, pid, exitcode, termsig } = cp;

  console.log(`ChildProcess`, { file, cwd, args, env, stdio, pid, exitcode, termsig });
}
const inspectOptions = {
  colors: true,
  showHidden: false,
  customInspect: true,
  showProxy: false,
  getters: false,
  depth: 4,
  maxArrayLength: 10,
  maxStringLength: 200,
  compact: 2,
  hideKeys: ['loc', 'range', 'inspect', Symbol.for('nodejs.util.inspect.custom')]
};

function main(...args) {
  console = new Console(inspectOptions);

  let cp = child_process.spawn('ls', ['-la'], { stdio: 'pipe' });

  DumpChildProcess(cp);

  let [stdin, stdout, stderr] = cp.stdio;
  console.log('stdio:', { stdin, stdout, stderr });

  let buf = new ArrayBuffer(4096);

/*  os.sleep(1);
  ret = os.read(stdout, buf, 0, buf.byteLength);
  ret = os.read(stdout, buf, 0, buf.byteLength);
  ret = os.read(stdout, buf, 0, buf.byteLength);
*/
  cp.wait();
   let ret = os.read(stdout, buf, 0, buf.byteLength);
  console.log('ret:', ret);
  if(ret > 0)
  console.log('buf:', toString(buf.slice(0,ret)));
DumpChildProcess(cp);
}

try {
  main(...scriptArgs.slice(1));
} catch(error) {
  console.log('FAIL:', error.message);
  std.exit(1);
} finally {
  console.log('SUCCESS');
  std.exit(0);
}
