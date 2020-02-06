$(function() {
	function Filament(data) {
		var self = this;
		self.date = ko.computed(function(){
			return data["date"];
		});
		self.fila = ko.computed(function(){
			if (data["fila"]=="") {
				return "unnamed";
			}
			else {
				return data["fila"];
			}
		});
		self.weight = ko.computed(function(){
			return data["weight"];
		});
		self.spool = ko.computed(function(){
			return data["spool"];
		});
		self.density = ko.computed(function(){
			return data["density"];
		});
		self.filaweight = ko.computed(function(){
			result= (data["weight"]-data["spool"]-data["coilweight"]);
			return result.toFixed(2);
		});
		self.units = ko.computed(function() {
			result = (data["weight"]-data["spool"]-data["coilweight"])/(data["density"]*2.41);
			return result.toFixed(2);
		});
	}
	function SmartScaleViewModel(parameters) {
		var self = this;
		self.printerState = parameters[0];
		self.settings = parameters[1];
		self.printerState.remainingstring = ko.observable("");
		self.navBarMessage = ko.observable();
		self.newref=ko.observable();
		self.onStartup = function(){
			var element = $("#state").find(".accordion-inner [data-bind='text: stateString']");
			if (element.length) {
				element.after("<br>Filament: <strong><div style='display: inline' data-bind='html: remainingstring'></div></strong>");
			};
		};
		self.tare = function(){
			OctoPrint.simpleApiCommand("SmartScale", "tare", {});
		};
		self.cali = function(){
			OctoPrint.simpleApiCommand("SmartScale", "cali", {"cali":self.settings.settings.plugins.SmartScale.referenceweight()});
		};
		self.startwifi = function() {
			OctoPrint.simpleApiCommand("SmartScale", "wifion", {});
		};
		self.ssid = function() {
			var ssid = prompt("Enter your networks Name:");
			var pass = prompt("Enter your networks Password:");
			OctoPrint.simpleApiCommand("SmartScale", "ssid", {"ssid":ssid, "pass":pass});
		};
		self.stopwifi = function() {
			OctoPrint.simpleApiCommand("SmartScale", "wifioff", {});
		};
		self.reconnect = function() {
			OctoPrint.simpleApiCommand("SmartScale", "reconnect", {});
		};
		self.loadFilament = function(index, filament) {
			self.settings.settings.plugins.SmartScale.activefilament(index());
			self.settings.settings.plugins.SmartScale.newFilament=filament.fila();
			self.settings.settings.plugins.SmartScale.spool_weight(filament.spool());
			self.settings.settings.plugins.SmartScale.material_density(filament.density());
			OctoPrint.simpleApiCommand("SmartScale", "load", {"spoolweight": filament.spool(),"dens": filament.density()});
			self.settings.saveData();
		};
		self.onDataUpdaterPluginMessage = function(plugin, data){
			if (plugin=="SmartScale") {
				if (data.hasOwnProperty("calcweight")) {
					self.printerState.remainingstring(parseFloat(data.length) + "m (" + parseFloat(data.calcweight) + "g)");
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
			};
		};
		self.addFilament = function() {
			datestring = new Date();
			self.settings.settings.plugins.SmartScale.filaments().push(
				new Filament({
					date: datestring.toLocaleString(),
					fila: self.settings.settings.plugins.SmartScale.newFilament,
					weight: parseFloat(self.settings.settings.plugins.SmartScale.settingsweight()),
					spool: parseFloat(self.settings.settings.plugins.SmartScale.spool_weight()),
					density: parseFloat(self.settings.settings.plugins.SmartScale.material_density()),
					coilweight: parseFloat(self.settings.settings.plugins.SmartScale.coilweight())
				})
			);
			self.settings.saveData();
		};
		self.removeFilament = function(index, filament) {
			if (index()<self.settings.settings.plugins.SmartScale.activefilament()) {
				self.settings.settings.plugins.SmartScale.activefilament(self.settings.settings.plugins.SmartScale.activefilament()-1);
			}
			self.settings.settings.plugins.SmartScale.filaments.remove(filament);
			self.settings.saveData();
		};
	};
	OCTOPRINT_VIEWMODELS.push({
		construct: SmartScaleViewModel,
		dependencies: ["printerStateViewModel", "settingsViewModel"],
		elements: ["#SmartScale_plugin_settings", "#SmartScale_plugin_navbar", "#filamenttab"]
	});
});
