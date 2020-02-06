# coding=utf-8
###
###
###
from __future__ import absolute_import
import sys
import time
import datetime
import os
import flask
import threading
import logging
import re
import glob
import serial
import octoprint.plugin
import octoprint.events

class SmartScalePlugin(
	octoprint.plugin.StartupPlugin,
	octoprint.plugin.TemplatePlugin,
	octoprint.plugin.SettingsPlugin,
	octoprint.plugin.EventHandlerPlugin,
	octoprint.plugin.AssetPlugin,
	octoprint.plugin.SimpleApiPlugin):
	def get_assets(self):
		return dict(
			js=["js/SmartScale.js"],
			css=["css/SmartScale.css"],
			less=["less/SmartScale.less"])
	def get_settings_defaults(self):
		return dict(
			referenceweight = 100,
			coilweight=100,
			spool_weight=50,
			altitude=59,
			containersize=10.6,
			heatertemp=0,
			filamentsonmain="0",
			navweight="0",
			navlength="0",
			navtemp="0",
			navhumi="0",
			navwater="0",
			navpressure="0",
			material_density=1.24,
			settingsweight=0,
			activefilament=0,
			filaments=[{"date": "01.01.2020, 01:01:01", "density": 1.24, "fila": "Pla", "filaweight": 0, "spool": 50, "units": 0, "weight": 0}],
			)
	def on_startup(self, host, port):
		self.error = ""
		self.navlist=[]
		self.spool_weight = float(self._settings.get(["spool_weight"]))
		self.density = float(float(self._settings.get(["material_density"]))*2.41)
		self.filaments = self._settings.get(["filaments"]);
		self.usbports = []
		self.thread = None
		self.usbCon = None
		self.settings_changed()
		self.connect()
	def connect(self):
		self.usbports = self.scanusb()
		if len(self.usbports) > 0:
			for port in self.usbports:
				if self.thread == None: and port != self._printer.get_current_connection()[1] and :
					try:
						self.usbCon = serial.Serial(port, 115200, timeout=0.5)
						self._logger.info("SmartScale connected to Port:" + port)
						line = self.usbCon.readline().rstrip('\r\n')
						if line:
							if line.startswith('[U:') and line.endswith(']'):
								self.thread = threading.Thread(target=self.readusb, args=())
								self.thread.daemon = True
								self.thread.start()
							else:
								self.usbCon = None
						else:
							self.usbCon = None
					except (OSError, serial.SerialException):
						self.usbCon = None
		if self.usbCon == None:
			self.error="No Scale found"
			self._plugin_manager.send_plugin_message(self._identifier, dict(remainingstring=self.error, weight="Error"))
	def scanusb(self):
		if sys.platform.startswith('win'):
			ports = ['COM%s' % (i + 1) for i in range(256)]
		elif sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
			ports = glob.glob('/dev/tty[A-Za-z]*')
		elif sys.platform.startswith('darwin'):
			ports = glob.glob('/dev/tty.*')
		else:
			raise EnvironmentError('Unsupported platform')
		result = []
		for port in ports:
			try:
				s = serial.Serial(port)
				s.close()
				result.append(port)
			except (OSError, serial.SerialException):
				pass
		return result
	def readusb(self):
		counter=0
		while self.usbCon != None:
			try:
				line = self.usbCon.readline().rstrip('\r\n')
				if line:
					if line.startswith('[U:') and line.endswith(']'):
						counter=0
						line = line[1:-1]
						feed = line.split(';')
						self._settings.set(['settingsweight'], feed[0][2:])
						self._settings.set(['settingscalcweight'], feed[1][2:])
						self._settings.set(['settingslength'], feed[2][2:])
						self._plugin_manager.send_plugin_message(self._identifier, dict(calcweight=feed[1][2:], length=feed[2][2:]))
						if len(self.navlist)>0:
							self.navbar = ""
							for item in self.navlist:
								self.navbar = self.navbar + feed[item]+" "
							self._plugin_manager.send_plugin_message(self._identifier, dict(navBarMessage=self.navbar))
				else:
					counter=counter+1
					if counter>60:
						self.usbCon = None
						if self.thread and threading.current_thread() != self.thread:
							self.thread.join()
						self.thread = None
						self.error = "Connection lost"
						break
			except (OSError, serial.SerialException):
				self.usbCon = None
				if self.thread and threading.current_thread() != self.thread:
					self.thread.join()
				self.thread = None
				self.error = "Connection lost"
		self._plugin_manager.send_plugin_message(self._identifier, dict(remainingstring=self.error, weight="Error"))
	def get_api_commands(self):
		return dict( reconnect=[], savefilaments=["filas"], load=["spoolweight", "dens"], tare=[], cali=["cali"], cont=[], alti=[], heat=[], wifion=[], wifioff=[], ssid=["ssid", "pass"])
	def on_api_command(self, command, data):
		import flask
		if command == 'reconnect':
			self.error=""
			self.connect()
		if self.usbCon != None:
			if command == 'ssid':
				self.usbCon.write(("<ssid:%s>" % data["ssid"]).encode())
				self.usbCon.write(("<pass:%s>" % data["pass"]).encode())
			if command == 'load':
				self.usbCon.write("<dens:%f>" % float(data["dens"]))
				self.usbCon.write("<spow:%.2f>" % float(data["spoolweight"]))
			if command == 'tare':
				self.usbCon.write("<tara:1>")
			if command == 'cali':
				self.usbCon.write("<cali:%.2f>" % float(data["cali"]))
			if command == 'cont':
				self.usbCon.write("<cont:%.2f>" % float(self._settings.get(["containersize"])))
			if command == 'alti':
				self.usbCon.write("<alti:%.2f>" % float(self._settings.get(["altitude"])))
			if command == 'heat':
				self.usbCon.write("<heat:%.2f>" % float(self._settings.get(["heatertemp"])))
			if command == 'wifion':
				self.usbCon.write("<wifi:1>")
			if command == 'wifioff':
				self.usbCon.write("<wifi:0>")
			if command == 'savefilaments':
				self._logger.info()
	def on_settings_save(self, data):
		octoprint.plugin.SettingsPlugin.on_settings_save(self, data)
		self.settings_changed()
	def settings_changed(self):
		self._plugin_manager.send_plugin_message(self._identifier, dict(navBarMessage=""))
		self.navlist=[]
		if self._settings.get(["navweight"])=="1":
			self.navlist.append(1)
		if self._settings.get(["navlength"])=="1":
			self.navlist.append(2)
		if self._settings.get(["navtemp"])=="1":
			self.navlist.append(3)
		if self._settings.get(["navhumi"])=="1":
			self.navlist.append(4)
		if self._settings.get(["navwater"])=="1":
			self.navlist.append(5)
		if self._settings.get(["navpressure"])=="1":
			self.navlist.append(6)
		if self.usbCon != None:
			self.usbCon.write("<coil:%.2f>" % float(self._settings.get(["coilweight"])))
			self.usbCon.write("<cont:%.2f>" % float(self._settings.get(["containersize"])))
			self.usbCon.write("<alti:%.2f>" % float(self._settings.get(["altitude"])))
			self.usbCon.write("<heat:%.2f>" % float(self._settings.get(["heatertemp"])))
	def on_event(self, event, payload):
		if event == "ClientAuthed":
			self._plugin_manager.send_plugin_message(self._identifier, dict(remainingstring=self.error, weight="Error"))
		if event in "PrintStarted":
			if isinstance(self._settings.get(["settingslength"]), float):
				self.job=self._printer.get_current_job()
				self.job=self.job['filament'][u'tool0'][u'length']
				self.job=float(self.job)/100
				if self.job>self._settings.get(["settingslength"]):
					self._logger.info("Alarm! ({0:.2f}/{1:.2f})".format(self._settings.get(["settingslength"]),self.job))
					self._plugin_manager.send_plugin_message(self._identifier, dict(alert="Printer Paused: Filament low ({0:.2f}/{1:.2f})".format(self._settings.get(["settingslength"]),self.job)))
					self._printer.pause_print()
					self._printer.pause_print()
					self._printer.pause_print()
					self._printer.pause_print()
			else:
				self._logger.info("Not engough Filament on Spool!")
	def get_update_information(self):
		return dict(
			SmartScalePlugin=dict(
				displayName="SmartScale",
				displayVersion=self._plugin_version
			)
		)
def __plugin_load__():
	global __plugin_implementation__
	__plugin_implementation__ = SmartScalePlugin()
	global __plugin_hooks__
	__plugin_hooks__ = {
		"octoprint.plugin.softwareupdate.check_config": __plugin_implementation__.get_update_information
	}
