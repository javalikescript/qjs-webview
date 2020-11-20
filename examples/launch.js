
// This script allows to launch a web page that could executes custom code on QuickJS.

import * as webview from '../webview.so'
import * as os from 'os'
import * as std from 'std'

let url = `data:text/html,<!DOCTYPE html>
<html>
  <body>
    <h1>Welcome !</h1>
    <p>You could specify an URL to launch as a command line argument.</p>
    <button onclick="window.external.invoke('terminate:')">Close</button>
  </body>
</html>
`

//print('path', os.realpath('.'), 'args', scriptArgs)

const target = scriptArgs[1];
const title = scriptArgs[2] || 'WebView';
const width = parseInt(scriptArgs[3] || '800', 10);
const height = parseInt(scriptArgs[4] || '600', 10);
const resizable = scriptArgs[5] === 'true';

if (target) {
  if (target === '-h') {
    print('Try', scriptArgs[0], 'url title width height resizable');
    std.exit(0);
  } else if (target.startsWith('/') || (target.substring(1, 3) == ':\\')) {
    url = 'file://' + target;
  } else if (target.startsWith('http') || target.startsWith('https') || target.startsWith('file') || target.startsWith('data')) {
    url = target;
  } else if (typeof os.realpath === 'function') {
    let path, err;
    [path, err] = os.realpath(target);
    if (err === 0) {
      url = 'file://' + path;
    } else {
      print('File not found (' + target + ')');
      std.exit(std.Error.ENOENT);
    }
  } else {
    print('Please specify an absolute filename or a valid URL');
    std.exit(std.Error.EINVAL);
  }
}

// Named requests callable from JS using window.external.invoke('name:value')
// Custom request can be registered using window.external.invoke('+name:JS code')
// The JS code has access to the string value
const requestFunctionMap = {
  // Toggles the web view full screen on/off
  fullscreen: function(value, requestContext, evalBrowser, callBrowser, webview) {
    webview.fullscreen(value === 'true');
  },
  // Terminates the web view
  terminate: function(value, requestContext, evalBrowser, callBrowser, webview) {
    webview.terminate(true);
  },
  // Sets the web view title
  title: function(value, requestContext, evalBrowser, callBrowser, webview) {
    webview.title(value);
  },
  // Evaluates the specified JS code in the browser
  evalBrowser: function(value, requestContext, evalBrowser, callBrowser, webview) {
    webview.eval(value, true);
  },
  // Executes the specified JS code in QuickJS
  eval: function(value, requestContext, evalBrowser, callBrowser, webview) {
    const fn = new Function('requestContext', 'evalBrowser', 'callBrowser', 'webview', value);
    fn(requestContext, evalBrowser, callBrowser, webview);
  },
}

// Defines a context that will be shared across requests
const requestContext = {}

// Setup a function to evaluates JS code in the browser
const evalBrowser = function(value) {
  w.eval(value, true);
}

// Setup a function to call a function in the browser
const callBrowser = function(functionName) {
  const jsArgs = [];
  for (let i = 1; i < arguments.length; i++) {
    jsArgs.push(JSON.stringify(arguments[i]));
  }
  const js = functionName + '(' + jsArgs.join(',') + ')';
  w.eval(js, true);
}

let w = webview.create(url, title, width, height, resizable)
w.init()
w.callback(function(request) {
  const colonIndex = request.indexOf(':');
  if (colonIndex <= 0) {
    print('invalid request', request);
    return;
  }
  const flag = request.charAt(0);
  const hasFlag = flag < 'A';
  const name = request.substring(hasFlag ? 1 : 0, colonIndex);
  const value = request.substring(colonIndex + 1);
  if (hasFlag) {
    switch(flag) {
    case '+':
      requestFunctionMap[name] = new Function('value', 'requestContext', 'evalBrowser', 'callBrowser', 'webview', value);
      print('added request function', name);
      break;
    case '-':
      delete requestFunctionMap[name];
      print('deleted request function', name);
      break;
    case '*':
      requestFunctionMap[name] = new Function('jsonValue', 'requestContext', 'evalBrowser', 'callBrowser', 'webview', 'const value = JSON.parse(jsonValue);' + value);
      print('added request function', name);
      break;
    default:
      print('unknown request flag', flag);
      break;
    }
  } else {
    const fn = requestFunctionMap[name];
    if (fn) {
      fn(value, requestContext, evalBrowser, callBrowser, w);
    } else {
      print('unknown request function', name);
    }
  }
})
w.loop();
//w = null;
