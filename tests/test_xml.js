import * as os from 'os';
import * as std from 'std';
import inspect from 'inspect.so';
import * as xml from 'xml.so';
import Console from './console.js';

'use strict';
'use math';

function WriteFile(file, data) {
  let f = std.open(file, 'w+');
  f.puts(data);
  console.log(`Wrote '${file}': ${data.length} bytes`);
}

async function main(...args) {
  console = new Console({ colors: true, depth: 1 });

  let winsz;

try {
  winsz = await os.ttyGetWinSize(1);
 }catch(err) {}
console.log('winsz:', winsz);

  let file = args[0] ?? 'Simple-NPN-Regen-Receiver.brd';
  console.log('file:', file);

  let base = file.replace(/.*\//g, '').replace(/\.[^.]*$/, '');
    console.log('base:', base);

  let data = std.loadFile(file, 'utf-8');
  console.log('data:', data.substring(0,100));

  // let result = parse2(Util.bufferToString(data));

  let result = xml.read(data);

  console.log('result:', result);

  let str =  xml.write(result);
  console.log('write:', str);

  console.log(`Writing '${base + '.xml'}'...`);
  WriteFile(base + '.xml', str);

  await import('std').then(std => std.gc());
}
console.log("test");
main(...scriptArgs.slice(1));