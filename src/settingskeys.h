#pragma once

#include <QString>
#include <QSettings>

// This file lists all the settings key used by Kvazzup.
// Using this file should reduce the chance of typos causing bugs.
// settings file names

const QString settingsFile = "kvazzup.ini";
const QString blocklistFile = "blocklist.local";
const QString contactsFile = "contacts.local";

const QSettings::Format settingsFileFormat = QSettings::IniFormat;


namespace SettingsKey {

// GUI setting keys

const QString screenShareStatus = "gui/ScreenShareEnabled";
const QString cameraStatus = "gui/CameraEnabled";
const QString micStatus = "gui/MicrophoneEnabled";


// Video setting keys
const QString videoDeviceID = "video/DeviceID";
const QString videoDevice = "video/Device";

const QString videoResultionWidth = "video/ResolutionWidth";
const QString videoResultionHeight = "video/ResolutionHeight";
const QString videoResolutionID = "video/ResolutionID";
const QString videoInputFormat = "video/InputFormat";
const QString videoYUVThreads = "video/yuvThreads";
const QString videoRGBThreads = "video/rgbThreads";
const QString videoOpenHEVCThreads = "video/OPENHEVC_threads";
const QString videoFramerateID = "video/FramerateID";
const QString videoFramerate = "video/Framerate";
const QString videoFlipEnabled = "video/flipEnabled";
const QString videoOpenGL = "video/opengl";


// Kvazaar setting keys
const QString videoQP = "video/QP";
const QString videoIntra = "video/Intra";
const QString videoSlices = "video/Slices";
const QString videoKvzThreads = "video/kvzThreads";
const QString videoWPP = "video/WPP";
const QString videoOWF = "video/OWF";
const QString videoTiles = "video/Tiles";
const QString videoTileDimensions = "video/tileDimensions";
const QString videoVPS = "video/VPS";
const QString videoBitrate = "video/bitrate";
const QString videoRCAlgorithm = "video/rcAlgorithm";
const QString videoOBAClipNeighbours = "video/obaClipNeighbours";
const QString videoScalingList = "video/scalingList";
const QString videoLossless = "video/lossless";
const QString videoMVConstraint = "video/mvConstraint";
const QString videoQPInCU = "video/qpInCU";
const QString videoVAQ = "video/vaq";
const QString videoPreset = "video/Preset";
const QString videoCustomParameters = "parameters";


// Audio setting keys
const QString audioDeviceID = "audio/DeviceID";
const QString audioDevice = "audio/Device";


// Opus encoding setting keys
const QString audioBitrate = "audio/bitrate";
const QString audioComplexity = "audio/complexity";
const QString audioSignalType = "audio/signalType";
// const QString audioChannels = "audio/channels";


// Speex DSP setting keys
const QString audioAEC = "audio/aec";
const QString audioAECDelay = "audio/aecPlaybackDelay";
const QString audioAECFilterLength = "audio/aecFilterLength";

const QString audioDenoise = "audio/denoise";
const QString audioDereverb = "audio/dereverb";
const QString audioAGC = "audio/agc";


// User setting keys
const QString userScreenID = "user/ScreenID";
const QString userScreen = "user/Screen";


// Local setting keys
const QString localRealname = "local/Name";
const QString localUsername = "local/Username";
const QString localAutoAccept = "local/Auto-Accept";

// SIP setting keys

const QString sipServerAddress = "sip/ServerAddress";
const QString sipAutoConnect = "sip/AutoConnect";

const QString sipMediaPort = "sip/mediaport";
const QString sipSTUNEnabled = "sip/stunEnabled";
const QString sipSTUNAddress = "sip/stunAddress";
const QString sipSTUNPort = "sip/stunPort";

// blocklist setting key
const QString blocklist = "blocklist";

// contact list setting key
const QString contactList = "contacts";
}
