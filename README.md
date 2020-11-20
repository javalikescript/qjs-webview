## Overview

The QuickJS webview module provides functions to open a web page in a dedicated window from QuickJS.

```js
import * as webview from 'webview.so'
webview.open("https://bellard.org/quickjs/", "QuickJS at bellard.org", 800, 600, true)
```

It uses *gtk-webkit2* on Linux and *MSHTML* (IE10/11) or *Edge* (Chromium) on Windows.

QuickJS can evaluate JavaScript code in the browser and the browser can call a registered QuickJS function, see the `simple.js` file in the examples.

This module is a binding of the tiny cross-platform [webview-c](https://github.com/javalikescript/webview-c) C library.

This module is part of the [qjls-dist](https://github.com/javalikescript/qjls-dist) project,
the binaries can be found on the [qjls](http://javalikescript.free.fr/quickjs/) page.

QuickJS webview is covered by the MIT license.

## Examples

Using the file system
```sh
qjs examples/launch.js examples/htdocs/calc.html "Calc" 320 400
```
