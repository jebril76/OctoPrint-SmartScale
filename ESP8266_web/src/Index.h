const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
  <body>
    <div id='top'>
      <h1>Filament - Box</h1>
      <div id='divsensor'>
      </div>
      <br>
      <div id='divshowhide'>
        <button type='button' onclick='settings("show")'>Einstellungen zeigen</button>    
      </div>
      <br>
      <br>
      <div id='divsettings'>
      </div>
    </div>
    <script>
      function settings(stat) {
        if (stat=="hide") {
          document.getElementById('divshowhide').innerHTML = "<button type='button' onclick='settings(\"show\")'>Einstellungen zeigen</button>";
          document.getElementById('divsettings').innerHTML = "";
        }
        else {
          var xhttp = new XMLHttpRequest();
          document.getElementById('divshowhide').innerHTML = "<button type='button' onclick='settings(\"hide\")'>Einstellungen verbergen</button>";
          xhttp.onreadystatechange = function() {
            if (this.readyState == 4 && this.status == 200) {
              document.getElementById('divsettings').innerHTML =
              this.responseText;
            }
          }
          xhttp.open('GET', 'showSettings?stat=' + stat, true);
          xhttp.send();
        }
      }
      function manSpool(spoolnr) {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
          if (this.readyState == 4 && this.status == 200) {
            document.getElementById('divsettings').innerHTML =
            this.responseText;
          }
        };
        xhttp.open('GET', 'manSpool?spoolnr='  + spoolnr, true);
        xhttp.send();
      }
      function manMat(matnr) {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
          if (this.readyState == 4 && this.status == 200) {
            document.getElementById('divsettings').innerHTML =
            this.responseText;
          }
        };
        xhttp.open('GET', 'manMat?matnr='  + matnr, true);
        xhttp.send();
      }
      function manSave(savenr) {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
          if (this.readyState == 4 && this.status == 200) {
            document.getElementById('divsettings').innerHTML =
            this.responseText;
          }
        };
        xhttp.open('GET', 'manSave?savenr='  + savenr, true);
        xhttp.send();
      }
      function saveMat(idname) {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
          if (this.readyState == 4 && this.status == 200) {
            document.getElementById('divsettings').innerHTML =
            this.responseText;
          }
        };
        var input = document.getElementById('matname' + idname);
        var name = encodeURIComponent(input.value);
        var input = document.getElementById('matdensity' + idname);
        var density = encodeURIComponent(input.value);
        xhttp.open('GET', 'saveMat?matnr=' + idname + '&matname='  + name + '&matdensity=' + density, true);
        xhttp.send();
      }
      function saveSpool(idname) {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
          if (this.readyState == 4 && this.status == 200) {
            document.getElementById('divsettings').innerHTML =
            this.responseText;
          }
        };
        var input = document.getElementById('spoolname' + idname);
        var name = encodeURIComponent(input.value);
        var input = document.getElementById('spoolweight' + idname);
        var weight = encodeURIComponent(input.value);
        xhttp.open('GET', 'saveSpool?spoolnr=' + idname + '&spoolname='  + name + '&spoolweight=' + weight, true);
        xhttp.send();
      }
      function saveSave(idname) {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
          if (this.readyState == 4 && this.status == 200) {
            document.getElementById('divsettings').innerHTML =
            this.responseText;
          }
        };
        var input = document.getElementById('savecolor' + idname);
        var name = encodeURIComponent(input.value);
        xhttp.open('GET', 'saveSave?savenr=' + idname + '&savecolor='  + name, true);
        xhttp.send();
      }
      function Scale(idname) {
        var xhttp = new XMLHttpRequest();
        var input = document.getElementById(idname);
        var inputData = encodeURIComponent(input.value);
        xhttp.open('GET', 'Scale?varname='  + idname + '&varval=' + inputData, true);
        xhttp.send();
      }
      setInterval(function() {
        // Call a function repetatively with 2 Second interval
        getData();
      }, 500); //2000m Seconds update rate
      function getData() {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
          if (this.readyState == 4 && this.status == 200) {
            document.getElementById('divsensor').innerHTML =
            this.responseText;
          }
        };
        xhttp.open('GET', 'updatePage', true);
        xhttp.send();
      }
    </script>
  </body>
</html>
)=====";