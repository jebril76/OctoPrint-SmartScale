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
import octoprint.filemanager
import octoprint.filemanager.util
from octoprint.util import RepeatedTimer
import ast
import json


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
			filamenttab="1",
			referenceweight = 1200,
			coilweight=180,
			altitude=59,
			containersize=10.6,
			heatertemp=0,
			navweight="0",
			navlength="0",
			navtemp="0",
			navhumi="0",
			navwater="0",
			navpressure="0",
			)
	def on_startup(self, host, port):
		self.navlist=[]
		self.filaments=[{"filaweight": 0, "weight": 0, "density": 1.24, "fila": "Pla", "spool": 50, "length": 100, "date": "01.01.2020, 01:01:01"}]
		self.active_filament=0
		self.filalength=10000
		self.filaments_load()
		self.usbCon = None
		self.settings_changed()
		self.connect()
	def connect(self):
		self.usbports = []
		self.thread = None
		self.error = ""
		self.usbports = self.scanusb()
		if len(self.usbports) > 0:
			for port in self.usbports:
				if self.thread == None and port != self._printer.get_current_connection()[1]:
					try:
						self.usbCon = serial.Serial(port, 115200, timeout=0.5)
						line = self.usbCon.readline()
						line = self.usbCon.readline().strip().decode('ascii')
						if line:
							if line.startswith('[U:') and line.endswith(']'):
								self._logger.info("SmartScale connected to Port:" + port)
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
			self._logger.info("SmartScale - " + self.error)
			self._plugin_manager.send_plugin_message(self._identifier, dict(error=self.error))
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
		timer=time.time()
		while self.usbCon != None:
			try:
				line = self.usbCon.readline().strip().decode('ascii')
##				self._logger.info(line)
				if line:
					if line.startswith('[U:') and line.endswith(']'):
						timer=time.time()
						feed = line[1:-1].split(';')
						self.filalength=feed[2][2:]
						self._plugin_manager.send_plugin_message(self._identifier, dict(weight=feed[0][2:], calcweight=feed[1][2:], length=feed[2][2:]))
						if len(self.navlist)>0:
							self.navbar = ""
							for item in self.navlist:
								self.navbar = self.navbar + feed[item]+" "
							self._plugin_manager.send_plugin_message(self._identifier, dict(navBarMessage=self.navbar))
					else:
						self._logger.info("SmartScale USB Connection Timeout " + time.time()-timer)
						if time.time()-timer>3:
							self.error = "Corrupt Data"
							break
				else:
					self._logger.info("SmartScale USB Connection Timeout " + time.time()-timer)
					if time.time()-timer>3:
						self.error = "No Data"
						break
			except (OSError, serial.SerialException):
				self.error = "Connection lost"
				break
		self.usbCon = None
		if self.thread and threading.current_thread() != self.thread:
			self.thread.join()
		self.thread = None
		self._logger.info("SmartScale - " + self.error)
		self._plugin_manager.send_plugin_message(self._identifier, dict(error=self.error))
		return false
	def get_api_commands(self):
		return dict( reconnect=[], savefilaments=["active_filament", "filaments"], load=["spoolweight", "dens"], tare=[], cali=["cali"], cont=[], alti=[], heat=[], wifion=[], wifioff=[], ssid=["ssid", "pass"])
	def on_api_command(self, command, data):
		import flask
		if command == 'reconnect' and self.usbCon == None:
			self.error=""
			self.connect()
		if self.usbCon != None:
			if command == 'ssid':
				self.usbCon.write(("<ssid:%s>" % data["ssid"]).encode())
				self.usbCon.write(("<pass:%s>" % data["pass"]).encode())
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
##			if command == 'wifion':
##				self.usbCon.write("<wifi:1>")
##			if command == 'wifioff':
##				self.usbCon.write("<wifi:0>")
			if command == 'load':
				self.usbCon.write("<dens:%.8f>" % float(data["dens"]))
				self.usbCon.write("<spow:%.2f>" % float(data["spoolweight"]))
			if command == 'savefilaments':
				self.filaments=json.loads(data["filaments"])
				self.active_filament=data["active_filament"]
				self.filaments_save()
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
			self.usbCon.write("<dens:%.6f>" % float(self.filaments[self.active_filament]["density"]))
			self.usbCon.write("<spow:%.2f>" % float(self.filaments[self.active_filament]["spool"]))
	def on_event(self, event, payload):
		if event == "ClientOpened" or event == "ClientAuthed":
			self._plugin_manager.send_plugin_message(self._identifier, dict(error=self.error))
			self._plugin_manager.send_plugin_message(self._identifier, dict(filaments=self.filaments, active_filament=self.active_filament))
		if event in "PrintStarted":
			if isinstance(self.filalength, float):
				self.job=self._printer.get_current_job()
				self.job=self.job['filament'][u'tool0'][u'length']
				self.job=float(self.job)/100
				if self.job>self.filalength:
					self._logger.info("Alarm! ({0:.2f}/{1:.2f})".format(self.filalength,self.job))
					self._plugin_manager.send_plugin_message(self._identifier, dict(alert="Printer Paused: Filament low ({0:.2f}/{1:.2f})".format(self.filalength,self.job)))
					self._printer.pause_print()
					self._printer.pause_print()
					self._printer.pause_print()
					self._printer.pause_print()
			else:
				self._logger.info("Not engough Filament on Spool!")
	def filaments_load(self):
		data_folder = self.get_plugin_data_folder()
		try:
			file_obj = open(data_folder + '/Filaments.ini', 'rb')
			self.active_filament=int(file_obj.readline().strip())
			self.filaments=json.loads(file_obj.readline())
			file_obj.close()
		except IOError:
			self._logger.info("Filaments File not found!")
			self.filaments_save()
		self._plugin_manager.send_plugin_message(self._identifier, dict(filaments=self.filaments, active_filament=self.active_filament))
	def filaments_save(self):
		data_folder = self.get_plugin_data_folder()
		try:
			file_obj = open(data_folder + '/Filaments.ini', 'wb')
			file_obj.write(str(self.active_filament) + "\n" + json.dumps(self.filaments, sort_keys=True))
			file_obj.close()
			self._plugin_manager.send_plugin_message(self._identifier, dict(filaments=self.filaments, active_filament=self.active_filament))
		except IOError:
			self._logger.info("Could not save Filaments File!")
	def get_update_information(self):
		return dict(
			SmartScalePlugin=dict(
				displayName="SmartScale",
				displayVersion=self._plugin_version,
				type="github_release",
				user="Jebril76",
				repo="OctoPrint-SmartScale",
				current=self._plugin_version,
				pip="https://github.com/jebril76/OctoPrint-SmartScale/archive/master.zip"
			)
		)
__plugin_name__ = "SmartScale"
__plugin_pythoncompat__ = ">=2.7,<4"
def __plugin_load__():
	global __plugin_implementation__
	__plugin_implementation__ = SmartScalePlugin()
	global __plugin_hooks__
	__plugin_hooks__ = {
		"octoprint.plugin.softwareupdate.check_config": __plugin_implementation__.get_update_information
	}
