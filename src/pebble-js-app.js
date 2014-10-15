var initialized = false;
var options = {};
var showSteps = 'on';
var showSeconds = 'off';
var updateInterval = 5;


Pebble.addEventListener("ready", function() {
  
  showSteps   = window.localStorage.getItem('showSteps')   ? window.localStorage.getItem('showSteps')   : 'on';
  showSeconds = window.localStorage.getItem('showSeconds') ? window.localStorage.getItem('showSeconds') : 'off';
  updateInterval = window.localStorage.getItem('updateInterval') ? window.localStorage.getItem('updateInterval') : 5;
  updateInterval = parseInt(updateInterval);
  initialized = true;
  console.log("ready called!" + showSteps + " - "+showSeconds+ " - "+updateInterval);
});

Pebble.addEventListener("showConfiguration", function() {
  console.log("showing configurationshowSteps="+showSteps+'&showSeconds='+showSeconds);
  Pebble.openURL('http://www.cylox-art.de/pebble/modernwatchstep.html?showSteps='+showSteps+'&showSeconds='+showSeconds+'&updateInterval='+updateInterval);
});

Pebble.addEventListener("webviewclosed", function(e) {
  console.log("configuration closed");
  // webview closed
  
  //Using primitive JSON validity and non-empty check
  if (e.response.charAt(0) == "{" && e.response.slice(-1) == "}" && e.response.length > 5) {
    options = JSON.parse(decodeURIComponent(e.response));
    
    showSteps = encodeURIComponent(options.SHOW_STEPS);
    window.localStorage.setItem('showSteps', showSteps);
    
    showSeconds = encodeURIComponent(options.SHOW_SECONDS);
    window.localStorage.setItem('showSeconds', showSeconds);
    
    updateInterval = parseInt(encodeURIComponent(options.UPDATE_INTERVAL));
    window.localStorage.setItem('updateInterval', updateInterval);
    var newOptions = {SHOW_STEPS:showSteps, SHOW_SECONDS:showSeconds, UPDATE_INTERVAL:updateInterval};
    
    console.log("Options = " + JSON.stringify(newOptions));
    
    //Send to Pebble, persist there
    Pebble.sendAppMessage(
      newOptions,
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

