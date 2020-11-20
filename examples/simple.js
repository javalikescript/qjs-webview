import * as webview from '../webview.so'

const content = `<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
  </head>
  <body>
    <p id="sentence">It works !</p>
    <button onclick="window.external.invoke('title=Changed Title')">Change Title</button>
    <button onclick="window.external.invoke('print_date')">Print Date</button>
    <button onclick="window.external.invoke('show_date')">Show Date</button>
    <br/>
    <button title="Reload" onclick="window.location.reload()">&#x21bb;</button>
    <button title="Toggle fullscreen" onclick="fullscreen = !fullscreen; window.external.invoke(fullscreen ? 'fullscreen' : 'exit_fullscreen')">&#x2922;</button>
    <button title="Terminate" onclick="window.external.invoke('terminate')">&#x2716;</button>
    <br/>
  </body>
  <script type="text/javascript">
  var fullscreen = false;
  </script>
</html>
`;

let w = webview.create('data:text/html,' + encodeURIComponent(content), "QuickJS WebView Example", 640, 480, true)

w.init()

w.callback(function(value) {
  if (value == 'print_date') {
    print(new Date());
  } else if (value == 'show_date') {
    w.eval('document.getElementById("sentence").innerHTML =  "QuickJS date is ' + (new Date()) + '"', true);
  } else if (value == 'fullscreen') {
    w.fullscreen(true);
  } else if (value == 'exit_fullscreen') {
    w.fullscreen(false);
  } else if (value == 'terminate') {
    w.terminate(true);
  } else if (value.startsWith('title=')) {
    w.title(value.substring(6));
  } else {
    print('callback received', value);
  }
})

w.loop();
//w = null;
