import * as webview from '../webview.so'

let w = webview.create("https://bellard.org/quickjs/", "QuickJS at bellard.org", 800, 600, true)
w.init()
w.loop();
w = null;
