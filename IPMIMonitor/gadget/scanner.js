// This script was based on HWiNFO64 by Martin Malik - REALiX.
var intHandle = 0;
var bodyHeight = 10;
var sRegkey = 'HKCU\\Software\\IPMIScanner\\';
var WshShell = new ActiveXObject("WScript.Shell");

function refreshGadget()
{
  bodyHeight = holder.offsetHeight;
  document.body.style.height = bodyHeight;
}

function updateGadget() {
  var h,j,k,s = "",a,b;
  
  try {
    WshShell.RegRead( sRegkey );
  } catch(err) {
    results.innerHTML = "<p>IPMIScanner not running.</p>";   
    return;
  }

  try {
    s = WshShell.RegRead( sRegkey + "gadget" );
    if (s == "") s = "<tr><td>Unavailable</td></tr>";
    else {
        a=s.split(";");
        s = "";
        j = a.length;
        if (j % 2) {
            a.push(",");
            ++j;
        } 
        h = j >> 1;
        for (k = 0; k < h;k++) {
            b=a[k].split(",");
            s += "<tr><td>" + b[0] + "</td><td class='scanVal'>" + b[1] + "&nbsp;</td>";
            s += "<td>&nbsp;</td>";
            b = a[k+h].split(",");
            s += "<td>" + b[0] + "</td><td class='scanVal'>" + b[1] + "</td></tr>";
        }
        a = null;
        b = null;
    }
  } catch(err) {
    s="<tr><td>Reg empty.</td></tr>";
  }

  results.innerHTML = '<table class="scanTable">' + s + '</table>';
  s = null;
}

function MainSetup() {
    updateGadget();
    intHandle = setInterval("updateGadget()", 4000);
    setInterval("refreshGadget()", 100);
}



