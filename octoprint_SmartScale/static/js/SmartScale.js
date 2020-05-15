$(function() {
// Define Filament
	function Filament(data) {
		var self = this;
		self.filaweight = ko.computed(function(){
			result= (data["weight"]-data["spool"]-data["coilweight"]);
			return result.toFixed(2);
		});
		self.weight = ko.computed(function(){
			return data["weight"];
		});
		self.density = ko.computed(function(){
			return data["density"];
		});
		self.fila = ko.computed(function(){
			if (data["fila"]=="" || !data["fila"]){
				return "unnamed";
			}
			else {
			return data["fila"];
			}
		});
		self.spool = ko.computed(function(){
			return data["spool"];
		});
		self.length = ko.computed(function(){
			result = (data["weight"]-data["spool"]-data["coilweight"])/(data["density"]*2.41);
			return result.toFixed(2);
		});
		self.date = ko.computed(function(){
			return data["date"];
		});
	};
	function SmartScaleViewModel(parameters){
// Vars for runtime
		var self = this;
		self.printerState = parameters[0];
		self.settings = parameters[1];
		self.printerState.remainingstring = ko.observable("");
		self.navBarMessage = ko.observable();
		self.newref=ko.observable();
		self.active_filament=ko.observable(0);
		self.newFilament=ko.observable("New");
		self.spool_weight=ko.observable(100);
		self.settingsweight=ko.observable(0);
		self.material_density=ko.observable(1.24);
		self.filaments=ko.observableArray();
// Modify Octoprint Status
		self.onStartup = function(){
			var element = $("#state").find(".accordion-inner [data-bind='text: stateString']");
			if (element.length) {
				element.after("<br>Filament: <strong><div style='display: inline' data-bind='html: remainingstring'></div></strong>");
			};
		};
		self.onAfterBinding = function(){
			if (self.settings.settings.plugins.SmartScale.filamenttab()=="0") {
				self.link=$('#tab_plugin_SmartScale_link').detach();
				self.tab=$('#filamenttab').detach();
				$('#tab_plugin_SmartScale').removeClass('active');
				$('#control_link').addClass("active");
				$('#control').addClass("active");
			}
			
		}
		self.onSettingsBeforeSave = function(){
			if (self.settings.settings.plugins.SmartScale.filamenttab()=="0") {
				self.link=$('#tab_plugin_SmartScale_link').removeClass('active').detach();
				self.tab=$('#filamenttab').detach();
				$('#tab_plugin_SmartScale').removeClass('active');
				$('#control_link').addClass("active");
				$('#control').addClass("active");
			} else {
				if (self.tab) {
					var bob = document.getElementById('SmartScale_filament');
					bob.id="filamenttab";
					self.tab=bob;
					self.tab.classList.remove('active');
				}
				$('#tabs').append(self.link);
				$('#filamenttab_place').append(self.tab);
				self.link = null;
				self.tab = null;
			}
		}
// Scale functions
		self.tare = function(){
			OctoPrint.simpleApiCommand("SmartScale", "tare", {});
		};
		self.cali = function(){
			OctoPrint.simpleApiCommand("SmartScale", "cali", {"cali":self.settings.settings.plugins.SmartScale.referenceweight()});
		};
/*		self.startwifi = function(){
			OctoPrint.simpleApiCommand("SmartScale", "wifion", {});
		};
		self.ssid = function(){
			var ssid = prompt("Enter your networks Name:");
			var pass = prompt("Enter your networks Password:");
			OctoPrint.simpleApiCommand("SmartScale", "ssid", {"ssid":ssid, "pass":pass});
		};
		self.stopwifi = function() {
			OctoPrint.simpleApiCommand("SmartScale", "wifioff", {});
		};
*/
		self.reconnect = function() {
			OctoPrint.simpleApiCommand("SmartScale", "reconnect", {});
		};
// Manage Spools
		self.loadFilament = function(index, filament) {
			self.active_filament(index());
			self.newFilament(self.filaments()[self.active_filament()].fila)
			self.spool_weight(self.filaments()[self.active_filament()].spool);
			self.material_density(self.filaments()[self.active_filament()].density);
			OctoPrint.simpleApiCommand("SmartScale", "load", {"spoolweight": self.spool_weight(), "dens": self.material_density()});
			OctoPrint.simpleApiCommand("SmartScale", "savefilaments", {"active_filament":index(), "filaments": ko.toJSON(self.filaments)});
		};
		self.addFilament = function() {
			datestring = new Date();
			self.filaments.remove( function (item) {
				return item.fila == self.newFilament();
			})
			if (self.settingsweight()=="Error") {
				var filweight = parseFloat(self.spool_weight()+self.settings.settings.plugins.SmartScale.coilweight())
			}
			else {
				var filweight = parseFloat(self.settingsweight())
			}
			self.filaments.push(
				new Filament({
					weight: filweight,
					density: parseFloat(self.material_density()),
					spool: parseFloat(self.spool_weight()),
					fila: self.newFilament(),
					date: datestring.toLocaleString(),
					coilweight: parseFloat(self.settings.settings.plugins.SmartScale.coilweight())
				})
			);
			OctoPrint.simpleApiCommand("SmartScale", "savefilaments", {"active_filament": self.filaments().length-1, "filaments": ko.toJSON(self.filaments)});
		};
		self.removeFilament = function(index, filament) {
			if (index()<self.active_filament()) {
				self.active_filament(self.active_filament()-1);
			}
			self.filaments.remove(filament);
			OctoPrint.simpleApiCommand("SmartScale", "savefilaments", {"active_filament":self.active_filament(), "filaments": ko.toJSON(self.filaments)});
		};
// Messages to User
		self.onDataUpdaterPluginMessage = function(plugin, data){
			if (plugin=="SmartScale") {
				if (data.hasOwnProperty("weight")) {
					self.printerState.remainingstring(parseFloat(data.length) + "m (" + parseFloat(data.calcweight) + "g)");
					self.settingsweight(data.weight);
				};
				if (data.hasOwnProperty("error")) {
					self.printerState.remainingstring(data.error);
					self.settingsweight("Error");
				};
				if (data.hasOwnProperty("navBarMessage")) {
					self.navBarMessage(data.navBarMessage);
				};
				if (data.hasOwnProperty("alert")) {
					new PNotify({
						title: 'Printer Alert',
						text: data.alert,
						type: 'error'
					});
				};
				if (data.hasOwnProperty("filaments")) {
					self.filaments(data.filaments);
					self.active_filament(data.active_filament);
					self.newFilament(self.filaments()[self.active_filament()]["fila"])
					self.spool_weight(self.filaments()[self.active_filament()]["spool"]);
					self.material_density(self.filaments()[self.active_filament()]["density"]);
				};
			};
		};
	};
	OCTOPRINT_VIEWMODELS.push({
		construct: SmartScaleViewModel,
		dependencies: ["printerStateViewModel", "settingsViewModel"],
		elements: ["#SmartScale_plugin_settings", "#SmartScale_plugin_navbar", "#filamenttab"]
	});
});
