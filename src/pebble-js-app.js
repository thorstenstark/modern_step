var initialized = false;
var options = {};
var showSteps = 'on';
Pebble.addEventListener("ready", function() {
  
  showSteps = window.localStorage.getItem('showSteps') ? window.localStorage.getItem('showSteps') : 'on';
  initialized = true;
  console.log("ready called!" + showSteps);
});

Pebble.addEventListener("showConfiguration", function() {
  console.log("showing configuration");
  Pebble.openURL('http://www.cylox-art.de/pebble/modernwatchstep.html?showSteps='+showSteps);//+encodeURIComponent(JSON.stringify(options)));
});

Pebble.addEventListener("webviewclosed", function(e) {
  console.log("configuration closed");
  // webview closed
  
  //Using primitive JSON validity and non-empty check
  if (e.response.charAt(0) == "{" && e.response.slice(-1) == "}" && e.response.length > 5) {
    options = JSON.parse(decodeURIComponent(e.response));
    
    showSteps = searchingFor = encodeURIComponent(options.SHOW_STEPS);
    window.localStorage.setItem('showSteps', searchingFor);
    
    console.log("Options = " + JSON.stringify(options));
    
    //Send to Pebble, persist there
    Pebble.sendAppMessage(
      options,
      function(e) {
        console.log("Sending settings data...");
      },
      function(e) {
        console.log("Settings feedback failed!");
      }
    );
  } else {
    console.log("Cancelled");
  }
});

