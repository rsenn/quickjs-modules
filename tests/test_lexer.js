import * as os from 'os';
import * as std from 'std';
import inspect from 'inspect.so';
import * as xml from 'xml.so';
import { Predicate } from 'predicate.so';
import { Lexer } from 'lexer.so';
import Console from './console.js';

('use strict');
('use math');

globalThis.inspect = inspect;

function WriteFile(file, data) {
  let f = std.open(file, 'w+');
  f.puts(data);
  console.log(`Wrote '${file}': ${data.length} bytes`);
}
function DumpLexer(lex) {
  const { size, pos, start, line, column, lineStart, lineEnd, columnIndex } = lex;

  return `Lexer ${inspect({ size, pos, start, line, column, lineStart, lineEnd, columnIndex })}`;
}
function DumpToken(tok) {
  const { length, offset, loc } = tok;

  return `Token ${inspect({ length, offset, loc }, { depth: Infinity })}`;
}

function main(...args) {
  console = new Console({
    colors: true,
    depth: 8,
    maxArrayLength: 100,
    maxStringLength: 100,
    compact: false
  });
  let str = std.loadFile(args[0] ?? scriptArgs[0], 'utf-8');
  let len = str.length;
  console.log('len', len);
  let lexer = new Lexer(str, len);

  /* prettier-ignore */ lexer.keywords = ['if', 'in', 'do', 'of', 'as', 'for', 'new', 'var', 'try', 'let', 'else', 'this', 'void', 'with', 'case', 'enum', 'from', 'break', 'while', 'catch', 'class', 'const', 'super', 'throw', 'await', 'yield', 'async', 'delete', 'return', 'typeof', 'import', 'switch', 'export', 'static', 'default', 'extends', 'finally', 'continue', 'function', 'debugger', 'instanceof'];
  /* prettier-ignore */ lexer.punctuators = [ '!', '!=', '!==', '${', '%', '%=', '&&', '&&=', '&', '&=', '(', ')', '*', '**', '**=', '*=', '+', '++', '+=', ',', '-', '--', '-->>', '-->>=', '-=', '.', '...', '/', '/=', ':', ';', '<', '<<', '<<=', '<=', '=', '==', '===', '=>', '>', '>=', '>>', '>>=', '>>>', '>>>=', '?', '?.', '??', '??=', '@', '[', '^', '^=', '{', '|', '|=', '||', '||=', '}', '~'];
  console.log('lexer', lexer);

  console.log('lexer', DumpLexer(lexer));
  //  console.log('lexer.peek()', lexer.peek());
  //console.log('lexer.next()', lexer.next());
  lexer.lexNumber = function lexNumber() {};
  lexer.stateFn = function lex() {
    console.log('stateFn');

    return lexer.stateFn;
  };
  lexer.acceptRun(c => /^[A-Za-z_]/.test(c));

  let data;
  for(let data of lexer) {
    console.log('data', data.toString());

    //      console.log(`peek() = '${lexer.peek()}'`);
    lexer.acceptRun(Lexer.isWhitespace);

    console.log('lexer', DumpLexer(lexer));
  }

  std.gc();
}

main(...scriptArgs.slice(1));