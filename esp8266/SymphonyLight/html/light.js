patterns = ["Flow", "Solid", "Fade", "Twinkle"];
colors=["RainbowColors","ForestColors","RedGreenBlue","OceanColors","OrangeWhiteBlue","Heat","GreenRedWhiteStripe","YellowBlueWhiteStripe","Christmas","PartyColors","RedWhiteBlue","CloudColors"];
//colors=["RainbowColors","Green","Blue","Red","Orange","Yellow","Rainbow","Green","Blue","Red","Orange","Yellow"];
staticPins = [14, 12, 13, 5];
CMD_PATTERNS = 0;
CMD_CONFIG = 4;
CMD_FIRE = 1;
var cfgJson;
var doReset = 0;
//var wsUri = "ws://"+location.hostname+"/ws";
//websocket = new WebSocket(wsUri);
//
//function wsHandler() {
//    websocket.onopen = function(evt) {
//      for (i=1;i<=2;i++) {
//        var hover = document.getElementById("tmp"+i);
//        hover.parentNode.removeChild(hover);
//      }
//    };
//    websocket.onclose = function(evt) {
//      status.innerHTML="DISCONNECTED";
//    };
//    websocket.onmessage = function(evt) {
//        if (evt.data != "Connected") {
//        	status.innerHTML="data arrived";
//        } else {
//          var status = document.getElementById('status');
//          status.innerHTML="Synchronized";
//        }
//    };
//    websocket.onerror = function(evt) {
//      console.log("ERROR: " + evt.data);
//    };
//  } 
function serverResponseHandler(json) {
	var cmd = json.cmd;
	switch(cmd) {
		case CMD_PATTERNS:
			prevIndex = 11;
			s = json.data;
//			if (s.index != 1) {
				prevIndex = s.index - 1;
//			}
			trPrev = document.getElementById("tr" + prevIndex);
			trPrev.style.backgroundColor=('transparent');
			if (s.index == 12)
				currentIndex = 0;
			else
				currentIndex = s.index;
			var tr = document.getElementById("tr"+currentIndex);
			tr.style.backgroundColor=('red');
			break;
		case CMD_FIRE:
			break;
		case CMD_CONFIG:
			cfgJson = json;
			showPins(cfgJson, false);
			break;
	}
}
/*
 * Sends data for the pick color
 */
function sendColor(rgb){
	var data = '{"core":7,"cmd":4,"data":['+rgb.r+","+rgb.g+","+rgb.b+"]}";
	websocket.send(data);
}
/*
 * Sends data for the light sequence
 */
function sendCycleData(){
	var data = '{"core":7,"cmd":1,"data":[';
	for (i = 0; i < colors.length; i++) {
		var chk = document.getElementById("in_"+i);	//the checkbox object
		if (chk.checked)
			data = data + '1';
		else
			data = data + '0';
		var r = document.getElementsByName("r"+i);	//radiobutton group
	    for (var j=0, len=r.length; j<len; j++) {
	    	if (r[j].checked) {
	    		if ( i == colors.length - 1)
	    			data = data + r[j].value +']}';
	    		else
	    			data = data + r[j].value + ',';
	    	}
	    }
	}
	var txt2 = document.getElementById("msg");
	txt2.value=data;
	websocket.send(data);
}
/*
 * sends data for fire sequence
 */
function sendFireData(){
	var data = '{"core":7,"cmd":3,"data":';
	var r = document.getElementsByName("rFire");	//radiobutton group
    for (var j=0, len=r.length; j<len; j++) {
    	if (r[j].checked) {
			data = data + r[j].value +'}';
    	}
    }
	var txt2 = document.getElementById("msg");
	txt2.value=data;
	websocket.send(data);
}
function sendSetSequence(){
	var seq = [{ ptrn:1, color:11, dur: 111}, { ptrn:2, color:22, dur: 222}];
	var obj = { core:7, cmd: 8, seq: seq};
	websocket.send(JSON.stringify(obj));
}
function sendGetSequence(){
	var obj = { core:7, cmd: 9};
	websocket.send(JSON.stringify(obj));
}
/*
 * sends data for sparkle sequence
 */
function sendSparkleData(){
	var data = '{"core":7,"cmd":6,"data":';
	var r = document.getElementsByName("rSparkle");	//radiobutton group
    for (var j=0, len=r.length; j<len; j++) {
    	if (r[j].checked) {
			data = data + r[j].value +'}';
    	}
    }
	var txt2 = document.getElementById("msg");
	txt2.value=data;
	websocket.send(data);
}
/**
 * Called when mouse hovers on the item
 * @param ito
 * @returns
 */
function hover(ito) {
	ito.style.backgroundColor = "gray";
}
/**
 * Called when mouse unhovers on the item
 * @param ito
 * @returns
 */
function unhover(ito) {
	ito.removeAttribute("style");
}
/*
 * Handles click events
 */
function handleClick(ito) {
	alert(ito.id);
}
/*
 * Handles change events
 */
function handleChange(ito) {
	alert("changed")
	doReset = 1;
}
/**
 * Loads the patterns page where user can select the different light effects 
 * @returns
 */
function loadPatterns(){
	var mainDiv = document.getElementById("main");
	mainDiv.innerHTML = "";
	var table = document.createElement('table');
	table.setAttribute("border","1");
	var th = document.createElement('th');
	var trh = document.createElement('tr');
	trh.append(th);
	for (i=0;i<patterns.length;i++) {
		var th1 = document.createElement('th');
		th1.setAttribute('style', "width:50px");
		th1.innerHTML = patterns[i];
		trh.append(th1);
	}
	table.append(trh);
	var th2 = document.createElement('th');
	var th3 = document.createElement('th');
	var p = document.createElement('p');
	for (i = 0; i < colors.length; i++) {
		var lbl = document.createElement('label');
		lbl.setAttribute('id', "lbl_"+i);
		lbl.textContent = colors[i];
		lbl.setAttribute('onclick',"handleClick(this)");
		lbl.setAttribute("onmouseover","hover(this)");
    	lbl.setAttribute("onmouseout","unhover(this)")
    	lbl.setAttribute("class","lbl");;
		var input = document.createElement('input');
		input.setAttribute('type',"checkbox");
		input.setAttribute('id', "in_"+i);
		input.setAttribute('checked',"true");
		input.setAttribute("class","chk");;
		var tdLbl = document.createElement('td');
		tdLbl.append(input);
		tdLbl.append(lbl);
		var tr = document.createElement('tr');
		tr.setAttribute('id', "tr"+i);
		tr.append(tdLbl);
		for (j=0;j<patterns.length;j++) {
			var td1 = document.createElement('td');
			td1.setAttribute('id', "td"+j+"_"+i);
			var r1 = document.createElement('input');
			r1.setAttribute('type',"radio");
			r1.setAttribute('name',"r"+i);
			r1.setAttribute("class","radio");;
			if (j==0)
				r1.setAttribute('checked',"checked");
			r1.setAttribute('id', "r"+i+"_"+j);
			r1.setAttribute('value', j);
			td1.append(r1);
			tr.append(td1);
		}
		table.append(tr);
	}
	p.appendChild(table);
	mainDiv.appendChild(p);
//	websocket.send('{"core":7,"cmd":7}');	//deprecated, we should use AJAX
	//TODO ajax here
}
/////////////////////// for the config
var stringToChange = 1;
function init() {
	  drawString();
}
function stringClick(ako) {
	alert(ako.id)
	alert(ako.innerHTML)
}
function drawString(ako) {
	e = document.getElementById("composer");
	var table = document.createElement('table');
	table.setAttribute("style","width:100%;");
//	table.setAttribute("border","1");
	e.appendChild(table);
	drawPixels(table, 2, 30, 1);
	drawPixels(table, 1, 60, 2);
	var br = document.createElement("br");
	document.getElementById("composer").appendChild(br);
}
function drawPixels(table, strings, numPixels, lightStyle) {
	var tr = document.createElement('tr');
	table.append(tr);
	var input = document.createElement('input');
	input.setAttribute('type',"radio");
	input.setAttribute('name',"lightStyle");
	input.setAttribute('value', lightStyle);
	input.setAttribute('strings', strings);
	var tdOpt = document.createElement('td');
	tr.appendChild(tdOpt);
	tdOpt.appendChild(input);
	for (j = 0; j < strings; j++) {
		var tdInfo = document.createElement('td');
		tr.append(tdInfo);
		sel = '<select id="lbl'+lightStyle+'_'+j+'U" style="width:5em;">'; 
		for (k = 1; k <= 8; k++) {
			sel += '<option value="'+k+'">'+k+'</option>';
		}
		sel += '</select>';
		tdInfo.innerHTML = 'Universe:';
		tdInfo.innerHTML += sel;
		tdInfo.innerHTML += '<br>Pixels:<label id="lbl'+lightStyle+'_'+j+'P">'+numPixels+'</label><br>Strings:<label id="lbl'+lightStyle+'_'+j+'S">'+strings+'</label><br>';
		var td = document.createElement('td');
		tr.append(td);
		var div = document.createElement("div");
		div.setAttribute('id',"divstring"+lightStyle);
		div.setAttribute('class',"menu");
		var a = document.createElement("a");
		a.setAttribute('class', "submenu");
		a.setAttribute('id',lightStyle);
		a.setAttribute("style","margin:10px 5px;height:50px;border-radius:0px;");
		div.appendChild(a);
		td.appendChild(div);
		var div2 = document.createElement("div");
		div2.setAttribute('id',"divstring"+lightStyle+"_"+j);
		div2.setAttribute("style","margin:0px 10px;display:inline-block;");
		div2.setAttribute("onClick","stringClick(this)");
		div2.setAttribute("onmouseover","hoverConfig(this)");
		div2.setAttribute("lbl", 'lbl'+lightStyle+'_'+j);
		div2.setAttribute('pix',numPixels);
		div2.setAttribute('s',strings);
		div2.setAttribute('u',lightStyle);
		a.appendChild(div2);
	  	for (i = 0; i<7; i++) {
	  		var p = document.createElement("P");
	  		p.setAttribute('id',"style"+lightStyle+"_"+i);
	  		//color = c[i % 3]
	  		if (i==0)
	  			p.innerHTML= 1;
	  		else if (i==6)
	  			p.innerHTML= numPixels/strings;
	  		else
	  			p.innerHTML= "=";
	  		p.setAttribute('class', "pStyle");  	  		
	  		p.setAttribute('style', "border-radius:10px;background-color:lightgreen;");
	  		div2.appendChild(p);
	  	}
	}
}
/**
 * Handles the Light Config response.
 * Response should contain the 
 *  pixelCount, universe, mirrored flag
 * @param xhttp - the response xhttp object
 * @returns
 */
function getConfigHandler(xhttp) {
	var cfgJson = JSON.parse(xhttp.responseText);
	showPins(cfgJson, false);
}
/*
 * Read the config data from the ESP
 */
function getConfig(){
//	var obj = {core:7, cmd: 2, cfg: 0};//cfg = 0; read config data?	//DEPRECATED sep 08 2019, we will use ajax
//	websocket.send(JSON.stringify(obj));
	sendToServer('GET', '/devConfig', getConfigHandler);
}
/*
 * Send the config data to the ESP
 */
function sendConfig(){
	var e = document.getElementById("txtS");
	var s = e.options[e.selectedIndex].value;
	var p = document.getElementById("txtPix").value;
	var u = parseInt(document.getElementById("txtU").value, 10);
	var rgb = document.getElementById("rgb").value;
	var mirror = 0;
	if (document.getElementById("chkM").checked)
		mirror = 1;
	var obj = { core:7, cmd: 2, cfg: 1, s:s, p:p, u:u, m:mirror, rst:doReset, rgb:rgb};//cfg = 1; send config data
	var arr = [];
	for (i = 0; i<s; i++) {
		var pin = parseInt(document.getElementById("pin"+i).value, 10);
		var fwd = 0;
		if (document.getElementById("fwd"+i).checked)
			fwd = 1;
		var data = {pin : pin, fwd : fwd};
		arr[i] = data;
	}
	obj.data = arr;
	websocket.send(JSON.stringify(obj));
	doReset = 0;
}
/**
 * Shows the pins for the string of pixels
 * Called when the txtS element is changed
 * @returns
 */
function refreshPins() {
	showPins(cfgJson, true);
}
/**
 * Shows the pins for the string of pixels
 * @returns
 */
function showPins(lcfgJson, isChanged) {
	document.getElementById("txtPix").value = lcfgJson.p;
	document.getElementById("txtU").value = lcfgJson.u;
	document.getElementById("chkM").checked = lcfgJson.m;
	var e = document.getElementById("txtS");
	var s = lcfgJson.s;
	if (isChanged) {
		savedS = s;
		s = e.options[e.selectedIndex].value;
		if (s > savedS) {
			for (i = savedS; i<s; i++) {
				var pin = staticPins[i];
				var fwd = lcfgJson.data[0].fwd;
					fwd = 1;
				var data = {pin : pin, fwd : fwd};
				lcfgJson.data[i] = data;
			}
		}
	}
	document.getElementById("txtS").value = s;
	var pins = document.getElementById("pins");
	pins.innerHTML = "";
	var table = document.createElement('table');
	table.setAttribute("border","1");
	var th0 = document.createElement('th');
	var th1 = document.createElement('th');
	th1.innerHTML = "pin";
	var th3 = document.createElement('th');
	th3.innerHTML = "fwd";
	var trh = document.createElement('tr');
	trh.append(th0);
	trh.append(th1);
	trh.append(th3);
	table.append(trh);
	pins.append(table);
	for (i = 0; i<s; i++) {
		var tr = document.createElement('tr');
		table.append(tr);
		var td0 = document.createElement('td');
		var td1 = document.createElement('td');
		var td3 = document.createElement('td');
		tr.append(td0);
		tr.append(td1);
		tr.append(td3);
		var lbl = document.createElement('label');
		lbl.innerHTML = "Str" + (i+1);
		td0.append(lbl);
		var input = document.createElement('input');
		input.setAttribute('type',"text");
		input.setAttribute('id',"pin" + i);
		input.setAttribute('style',"width:3em");
		input.setAttribute('value', lcfgJson.data[i].pin);
		input.disabled = true;;
		td1.append(input);
		var input3 = document.createElement('input');
		input3.setAttribute('type',"checkbox");
		if (lcfgJson.data[i].fwd==1)
			input3.checked = true;
		else
			input3.checked = false;
		input3.setAttribute('id',"fwd" + i);
		td3.append(input3);
	}
}
function hoverConfig(ako) {
	var lbl = ako.getAttribute("lbl");
	var txt  = document.getElementById("txtU");
	txt.innerHTML = lbl;	
}
/**
 * used to debounce the events
 * 
 * @param func
 * @param wait
 * @param immediate
 * @returns
 */
function debounce(func, wait, immediate) {
	var timeout;
	return function() {
		var context = this, args = arguments;
		var later = function() {
			timeout = null;
			if (!immediate) func.apply(context, args);
		};
		var callNow = immediate && !timeout;
		clearTimeout(timeout);
		timeout = setTimeout(later, wait);
		if (callNow) func.apply(context, args);
	};
};