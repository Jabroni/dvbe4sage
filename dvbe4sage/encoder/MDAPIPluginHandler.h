#pragma once

#include "mdapi.h"
#include "pluginshandler.h"

class Plugin
{
	friend class MDAPIPluginsHandler;
	HMODULE								m_hModule;
	HMENU								m_hMenu;
	Plugin_Init_Proc					m_fpInit;
	Plugin_Channel_Change_Proc			m_fpChannelChange;
	Plugin_Exit_Proc					m_fpExit;
	Plugin_Hot_Key_Proc					m_fpHotKey;
	Plugin_OSD_Key_Proc					m_fpOSDKey;
	Plugin_Menu_Cmd_Proc				m_fpMenuCommand;
	Plugin_Send_Dll_ID_Name_Proc		m_fpGetPluginName;
	Plugin_Filter_Close_Proc			m_fpFilterClose;
	Plugin_Extern_RecPlay_Proc			m_fpExternRecPlay;
public:
	virtual bool acceptsCAid(const hash_set<CAScheme>& caid) { return true; }
};

class MDAPIPluginsHandler : public PluginsHandler
{
protected:

	// Global data structures
	list<Plugin>							m_Plugins;
	TMDAPIFilterProc						m_CurrentEcmCallback;
	TMDAPIFilterProc						m_CurrentEmmCallback;
	TMDAPIFilterProc						m_CurrentPmtCallback;
	TMDAPIFilterProc						m_CurrentCatCallback;
	USHORT									m_CurrentEcmFilterId;
	USHORT									m_CurrentEmmFilterId;
	USHORT									m_CurrentPmtFilterId;
	USHORT									m_CurrentCatFilterId;

	// Fill the TP structure function
	void fillTPStructure(LPTPROGRAM82 tp) const;

	// Callbacks
	void startFilter(LPARAM lParam);
	void stopFilter(LPARAM lParam);
	void dvbCommand(LPARAM lParam);

protected:

  virtual void processECMPacket(BYTE* packet);
  virtual void processEMMPacket(BYTE* packet);
  virtual void processPMTPacket(BYTE* packet);
  virtual void processCATPacket(BYTE* packet);
  virtual void resetProcessState();
  virtual bool canProcessECM()					{ return m_CurrentEcmCallback != NULL; }
  virtual bool handleTuningRequest();

public:
	// Constructor
	MDAPIPluginsHandler(HINSTANCE hInstance, HWND hWnd, HMENU hParentMenu);
	// Destructor
	virtual ~MDAPIPluginsHandler();
	// Window procedure for handling messages
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
};
