/*
 Copyright 2016 Herko ter Horst
 __________________________________________________________________________

 Filename: _androidautoApp.js
 __________________________________________________________________________
 */

log.addSrcFile("_androidautoApp.js", "_androidauto");

function _androidautoApp(uiaId)
{
    log.debug("Constructor called.");

    // Base application functionality is provided in a common location via this call to baseApp.init().
    // See framework/js/BaseApp.js for details.
    baseApp.init(this, uiaId);
    
//    framework.sendEventToMmui("common", "SelectBTAudio");

}


/*********************************
 * App Init is standard function *
 * called by framework           *
 *********************************/

/*
 * Called just after the app is instantiated by framework.
 * All variables local to this app should be declared in this function
 */
_androidautoApp.prototype.appInit = function()
{
    log.debug("_androidautoApp appInit  called...");

    //Context table
    //@formatter:off
    this._contextTable = {
        "Start": { // initial context must be called "Start"
            "sbName": "Android Auto",
            "hideHomeBtn" : true,
            "template": "AndroidAutoTmplt",
            "properties" : {
				"customBgImage" : "common/images/FullTransparent.png",
                "keybrdInputSurface" : "TV_TOUCH_SURFACE", 
                "visibleSurfaces" :  ["TV_TOUCH_SURFACE"]    // Do not include JCI_OPERA_PRIMARY in this list            
            },// end of list of controlProperties
            "templatePath": "apps/_androidauto/templates/AndroidAuto", //only needed for app-specific templates
            "readyFunction": this._StartContextReady.bind(this),
            "contextOutFunction" : this._StartContextOut.bind(this),
            "noLongerDisplayedFunction" : this._StartContextOut.bind(this)
        } // end of "AndroidAuto"
    }; // end of this.contextTable object
    //@formatter:on

    //@formatter:off
    this._messageTable = {
        // haven't yet been able to receive messages from MMUI
    };
    //@formatter:on

    var timerId = null;
};

/**
 * =========================
 * CONTEXT CALLBACKS
 * =========================
 */

function callCommandServer(method, request)
{
    var xhttp = new XMLHttpRequest();
    xhttp.open(method, "http://localhost:8000/" + request, false);
    xhttp.send();

    if (xhttp.readState == 4 && xhttp.stats == 200)
    {
        return JSON.parse(xhttp.responseText);
    }
    return null;
};

function AAlogPoll() {
	
    var currentStatus = callCommandServer("GET", "status");
    //no point updating if not showing this pane
    if (!currentStatus.videoFocus)
    {
        //put these back to what the JS code thinks they are just incase
        utility.setRequiredSurfaces(framework._visibleSurfaces, true);

        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() 
        {
            var debugTxt = "";
            if (this.readyState == 4 && this.status == 200) 
            {
                debugTxt = xmlhttp.responseText;
            }
            else 
            {
                debugTxt = "HTTP Error";
            }
            var psconsole = document.getElementById('aaStatusText');
            if (psconsole.value != debugTxt)
            {
                psconsole.focus();
                psconsole.value = debugTxt;

                if(psconsole.length)
                    psconsole.scrollTop(psconsole[0].scrollHeight - psconsole.height());
            }
            
        };
        xhttp.open("GET", "file:///tmp/mnt/data/headunit.log", true);
        xhttp.send();
    }

} 

_androidautoApp.prototype._StartContextReady = function ()
{
    AAlogPoll();
    if (timerId == null)
    {
        timerId = window.setInterval(AAlogPoll, 1000);
    }

    var currentStatus = callCommandServer("GET", "status");
    if (!currentStatus.videoFocus && currentStatus.connected)
    {
        callCommandServer("POST", "takeVideoFocus");
    }
}; 

_androidautoApp.prototype._StartContextOut = function ()
{
    if (timerId != null)
    {
        window.clearInterval(timerId);
        timerId = null;
    }
};



/**
 * =========================
 * Framework register
 * Tell framework this .js file has finished loading
 * =========================
 */
framework.registerAppLoaded("_androidauto", null, false);
