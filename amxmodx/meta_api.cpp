/* AMX Mod X
*
* by the AMX Mod X Development Team
*  originally developed by OLO
*
*
*  This program is free software; you can redistribute it and/or modify it
*  under the terms of the GNU General Public License as published by the
*  Free Software Foundation; either version 2 of the License, or (at
*  your option) any later version.
*
*  This program is distributed in the hope that it will be useful, but
*  WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
*  General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software Foundation,
*  Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*
*  In addition, as a special exception, the author gives permission to
*  link the code of this program with the Half-Life Game Engine ("HL
*  Engine") and Modified Game Libraries ("MODs") developed by Valve,
*  L.L.C ("Valve"). You must obey the GNU General Public License in all
*  respects for all of the code used other than the HL Engine and MODs
*  from Valve. If you modify this file, you may extend this exception
*  to your version of the file, but you are not obligated to do so. If
*  you do not wish to do so, delete this exception statement from your
*  version.
*/

#include <extdll.h>
#include <meta_api.h>
#include "amxmod.h"

plugin_info_t Plugin_info = {
  META_INTERFACE_VERSION, // ifvers
  "AMX Mod X",  // name
  AMX_VERSION,  // version
  __DATE__, // date
  "AMX Mod X Dev Team", // author
  "http://www.amxmodx.org",  // url
  "AMXX",  // logtag
  PT_ANYTIME,// (when) loadable
  PT_ANYTIME,// (when) unloadable
};

meta_globals_t *gpMetaGlobals;
gamedll_funcs_t *gpGamedllFuncs;
mutil_funcs_t *gpMetaUtilFuncs;
enginefuncs_t g_engfuncs;
globalvars_t  *gpGlobals;

funEventCall modMsgsEnd[MAX_REG_MSGS];
funEventCall modMsgs[MAX_REG_MSGS];
void (*function)(void*);
void (*endfunction)(void*);

CForwardMngr  g_forwards;
CList<CPlayer*> g_auth;
CList<CCVar> g_cvars;
CList<ForceObject> g_forcemodels;
CList<ForceObject> g_forcesounds;
CList<ForceObject> g_forcegeneric;
CPlayer g_players[33];
CPlayer* mPlayer;
CPluginMngr g_plugins;
CTaskMngr g_tasksMngr;
CmdMngr g_commands;
EventsMngr g_events;
Grenades g_grenades;
LogEventsMngr g_logevents;
MenuMngr g_menucmds;
String g_log_dir;
String g_mod_name;
XVars g_xvars;
bool g_bmod_cstrike;
bool g_bmod_dod;
bool g_dontprecache;
bool g_forcedmodules;
bool g_forcedsounds;
bool g_initialized;
fakecmd_t g_fakecmd;
float g_game_restarting;
float g_game_timeleft;
float g_task_time;
float g_auth_time;
hudtextparms_t g_hudset;
//int g_edict_point;
int g_players_num;
int mPlayerIndex;
int mState;
int g_srvindex;

cvar_t  init_amx_version={"amx_version","", FCVAR_SERVER | FCVAR_SPONLY};
cvar_t  init_amxmodx_version={"amxmodx_version","", FCVAR_SERVER | FCVAR_SPONLY};
cvar_t* amx_version = NULL;
cvar_t* amxmodx_version = NULL;
cvar_t* hostname = NULL;
cvar_t* mp_timelimit = NULL;

// Precache stuff from force consistency calls
// or check for pointed files won't be done
int PrecacheModel(char *s) {
  if ( !g_forcedmodules ){
    g_forcedmodules = true;
    for(CList<ForceObject>::iterator a =  g_forcemodels.begin(); a ; ++a){
      PRECACHE_MODEL((char*)(*a).getFilename());
      ENGINE_FORCE_UNMODIFIED((*a).getForceType(),(*a).getMin(),(*a).getMax(),(*a).getFilename());
    }
  }
  RETURN_META_VALUE(MRES_IGNORED, 0);
}

int PrecacheSound(char *s) {
  if ( !g_forcedsounds ) {
    g_forcedsounds = true;
    for(CList<ForceObject>::iterator a =  g_forcesounds.begin(); a ; ++a){
      PRECACHE_SOUND((char*)(*a).getFilename());
      ENGINE_FORCE_UNMODIFIED((*a).getForceType(),(*a).getMin(),(*a).getMax(),(*a).getFilename());
    }
    if (!g_bmod_cstrike){
      PRECACHE_SOUND("weapons/cbar_hitbod1.wav");
      PRECACHE_SOUND("weapons/cbar_hitbod2.wav");
      PRECACHE_SOUND("weapons/cbar_hitbod3.wav");
    }
  }
  RETURN_META_VALUE(MRES_IGNORED, 0);
}

// On InconsistentFile call forward function from plugins
int InconsistentFile( const edict_t *player, const char *filename, char *disconnect_message )
{
  if ( !g_forwards.forwardsExist( FF_InconsistentFile ) )
    RETURN_META_VALUE(MRES_IGNORED, FALSE);

  if ( MDLL_InconsistentFile(player,filename,disconnect_message) )
  {
    cell ret = 0;
    CPlayer *pPlayer = GET_PLAYER_POINTER((edict_t *)player);
    CForwardMngr::iterator a = g_forwards.begin( FF_InconsistentFile  );

#ifdef ENABLEEXEPTIONS
    try{
#endif

      while ( a )
      {

        if ( (*a).getPlugin()->isExecutable( (*a).getFunction()  ) )
        {
          AMX* c = (*a).getPlugin()->getAMX();
          cell amx_addr1, *phys_addr1;
          cell amx_addr2, *phys_addr2;
          if ((amx_Allot(c, 64 , &amx_addr1, &phys_addr1) != AMX_ERR_NONE) ||
            (amx_Allot(c, 64 , &amx_addr2, &phys_addr2) != AMX_ERR_NONE) ){
            UTIL_Log("[AMXX] Failed to allocate AMX memory (plugin \"%s\")",(*a).getPlugin()->getName());
          }
          else {
            int err;
            set_amxstring(c,amx_addr1,filename,63);
            set_amxstring(c,amx_addr2,disconnect_message,63);
            if ((err = amx_Exec(c,&ret, (*a).getFunction() , 3, pPlayer->index, amx_addr1, amx_addr2)) != AMX_ERR_NONE)
              UTIL_Log("[AMXX] Run time error %d on line %ld (plugin \"%s\")",
              err,c->curline,(*a).getPlugin()->getName());
            int len;
            strcpy(disconnect_message,get_amxstring(c,amx_addr2,0,len));
            amx_Release(c, amx_addr2);
            amx_Release(c, amx_addr1);
          }
          if ( ret & 1 ) RETURN_META_VALUE(MRES_SUPERCEDE, FALSE);
        }


        ++a;
      }
#ifdef ENABLEEXEPTIONS
    }catch( ... )
    {
      UTIL_Log( "[AMXX] fatal error at inconsistent file forward execution");
    }
#endif

    RETURN_META_VALUE(MRES_SUPERCEDE, TRUE );
  }

  RETURN_META_VALUE(MRES_IGNORED, FALSE);
}

const char* get_localinfo( const char* name , const char* def )
{
  const char* b = LOCALINFO( (char*)name );
  if ( b == 0 || *b == 0 )
    SET_LOCALINFO((char*)name,(char*)(b = def) );
  return b;
}

// Very first point at map load
// Load AMX modules for new native functions
// Initialize AMX stuff and load it's plugins from plugins.ini list
// Call precache forward function from plugins
int Spawn( edict_t *pent ) {

  if ( g_initialized ) RETURN_META_VALUE(MRES_IGNORED, 0);

  g_initialized = true;
  g_forcedmodules = false;
  g_forcedsounds = false;

  g_srvindex = IS_DEDICATED_SERVER() ? 0 : 1;

  hostname = CVAR_GET_POINTER("hostname");
  mp_timelimit = CVAR_GET_POINTER("mp_timelimit");

  // ###### Initialize logging
  g_log_dir.set( get_localinfo("amx_logdir" , "addons/amxx/logs" ) );
  UTIL_MakeNewLogFile();

  // ###### Initialize task manager
  g_tasksMngr.registerTimers( &gpGlobals->time, &mp_timelimit->value,  &g_game_timeleft     );

  // ###### Initialize commands prefixes
  g_commands.registerPrefix( "amx" );
  g_commands.registerPrefix( "say" );
  g_commands.registerPrefix( "admin_" );
  g_commands.registerPrefix( "sm_" );
  g_commands.registerPrefix( "cm_" );

  Vault amx_config;
  // ###### Load custom path configuration
  amx_config.setSource( build_pathname("%s",
    get_localinfo("amxx_cfg" , "addons/amxx/configs/core.ini")) );

  if ( amx_config.loadVault() ){
    Vault::iterator a = amx_config.begin();
    while ( a != amx_config.end() ) {
      SET_LOCALINFO( (char*)a.key().str() , (char*)a.value().str() );
      ++a;
    }
    amx_config.clear();
  }

  //  ###### Make sure basedir is set
  get_localinfo("amxx_basedir" , "addons/amxx" );

  //  ###### Load modules
  int loaded = loadModules( get_localinfo("amxx_modules" , "addons/amxx/modules.ini" ) );
  attachModules();
  // Set some info about amx version and modules
  if ( loaded ){
    char buffer[64];
    sprintf( buffer,"%s (%d module%s)",
      AMX_VERSION, loaded , (loaded == 1) ? "" : "s" );
    CVAR_SET_STRING( "amx_version" , buffer );
	CVAR_SET_STRING( "amxmodx_version", buffer);
  }
  else {
    CVAR_SET_STRING( "amx_version", AMX_VERSION );
	CVAR_SET_STRING( "amxmodx_version", AMX_VERSION );
  }

  //  ######  Load Vault
  g_vault.setSource( build_pathname("%s",
    get_localinfo("amxx_vault" , "addons/amxx/configs/vault.ini" ) ) );
  g_vault.loadVault( );


  //  ###### Init time and freeze tasks
  g_game_timeleft = g_bmod_dod ? 1 : 0;
  g_task_time = gpGlobals->time + 99999.0;
  g_auth_time = gpGlobals->time + 99999.0;
  g_players_num = 0;

  // Set server flags
  memset(g_players[0].flags,-1,sizeof(g_players[0].flags));

  //  ###### Load AMX scripts
  g_plugins.loadPluginsFromFile(
    get_localinfo("amxx_plugins" , "addons/amxx/plugins.ini" )  );

  //  ###### Call precache forward function
  g_dontprecache = false;
  g_forwards.executeForwards(FF_PluginPrecache);
  g_dontprecache = true;

  for(CList<ForceObject>::iterator a =  g_forcegeneric.begin(); a ; ++a){
      PRECACHE_GENERIC((char*)(*a).getFilename());
      ENGINE_FORCE_UNMODIFIED((*a).getForceType(),
      (*a).getMin(),(*a).getMax(),(*a).getFilename());
  }


  RETURN_META_VALUE(MRES_IGNORED, 0);
}

struct sUserMsg {
  const char* name;
  int* id;
  funEventCall func;
  bool endmsg;
  bool cstrike;
} g_user_msg[] = {
  { "CurWeapon" , &gmsgCurWeapon , Client_CurWeapon, false,false },
  { "Damage" , &gmsgDamage,Client_DamageEnd, true , true },
  { "DeathMsg" , &gmsgDeathMsg, Client_DeathMsg, false,true },
  { "TextMsg" , &gmsgTextMsg,Client_TextMsg , false,false},
  { "TeamInfo" , &gmsgTeamInfo,Client_TeamInfo , false,false},
  { "WeaponList" , &gmsgWeaponList, Client_WeaponList, false, false},
  { "MOTD" , &gmsgMOTD, 0 , false,false},
  { "ServerName" , &gmsgServerName, 0 , false, false},
  { "Health" , &gmsgHealth, 0 , false,false },
  { "Battery" , &gmsgBattery, 0 , false,false},
  { "ShowMenu" , &gmsgShowMenu,Client_ShowMenu , false,false},
  { "SendAudio" , &gmsgSendAudio, 0, false,false},
  { "AmmoX" , &gmsgAmmoX, Client_AmmoX , false,false },
  { "ScoreInfo" , &gmsgScoreInfo, Client_ScoreInfo, false, false},
  { "VGUIMenu" , &gmsgVGUIMenu,Client_VGUIMenu, false,false },
  { "AmmoPickup" , &gmsgAmmoPickup, Client_AmmoPickup , false,false },
  { "WeapPickup" , &gmsgWeapPickup,0, false,false },
  { "ResetHUD" , &gmsgResetHUD,0, false,false },
  { "RoundTime" , &gmsgRoundTime,0, false, false},
  { 0 , 0,0,false,false }
};


int RegUserMsg_Post(const char *pszName, int iSize)
{
  for (int i = 0; g_user_msg[ i ].name; ++i )
  {
    if ( strcmp( g_user_msg[ i ].name , pszName  ) == 0 )
    {
      int id = META_RESULT_ORIG_RET( int );

      *g_user_msg[ i ].id = id;

      if ( !g_user_msg[ i ].cstrike || g_bmod_cstrike  )
      {
        if ( g_user_msg[ i ].endmsg )
          modMsgsEnd[ id  ] = g_user_msg[ i ].func;
        else
          modMsgs[ id  ] = g_user_msg[ i ].func;
      }

      break;
    }
  }

  RETURN_META_VALUE(MRES_IGNORED, 0);
}

/*
Much more later after precache. All is precached, server
will be flaged as ready to use so call
plugin_init forward function from plugins
*/
void ServerActivate( edict_t *pEdictList, int edictCount, int clientMax ){

  int id;
  for (int i = 0; g_user_msg[ i ].name; ++i )
  {
    if ( (*g_user_msg[ i ].id == 0) &&
      (id = GET_USER_MSG_ID(PLID, g_user_msg[ i ].name , NULL ))!=0)
    {
      *g_user_msg[ i ].id = id;

      if ( !g_user_msg[ i ].cstrike || g_bmod_cstrike  )
      {
        if ( g_user_msg[ i ].endmsg )
          modMsgsEnd[ id  ] = g_user_msg[ i ].func;
        else
          modMsgs[ id  ] = g_user_msg[ i ].func;
      }
    }
  }

  RETURN_META(MRES_IGNORED);
}

void ServerActivate_Post( edict_t *pEdictList, int edictCount, int clientMax ){

//  g_edict_point = (int)pEdictList;

  for(int i = 1; i <= gpGlobals->maxClients; ++i) {
    CPlayer *pPlayer = GET_PLAYER_POINTER_I(i);
    pPlayer->Init( pEdictList + i , i );
  }

  g_forwards.executeForwards(FF_PluginInit);
  g_forwards.executeForwards(FF_PluginCfg);

  // Correct time in Counter-Strike and other mods (except DOD)
  if ( !g_bmod_dod)  g_game_timeleft = 0;

  g_task_time = gpGlobals->time;
  g_auth_time = gpGlobals->time;

  RETURN_META(MRES_IGNORED);
}

// Call plugin_end forward function from plugins.
void ServerDeactivate() {

  for(int i = 1; i <= gpGlobals->maxClients; ++i){
    CPlayer *pPlayer = GET_PLAYER_POINTER_I(i);
    if (pPlayer->ingame){

      g_forwards.executeForwards(FF_ClientDisconnect , 1,pPlayer->index);

      pPlayer->Disconnect();
      --g_players_num;
    }
  }

  g_players_num = 0;
  g_forwards.executeForwards(FF_PluginEnd);

  RETURN_META(MRES_IGNORED);
}

// After all clear whole AMX configuration
// However leave AMX modules which are loaded only once
void ServerDeactivate_Post() {

  g_initialized = false;

  dettachReloadModules();

  g_auth.clear();
  g_forwards.clear();
  g_commands.clear();
  g_forcemodels.clear();
  g_forcesounds.clear();
  g_forcegeneric.clear();
  g_grenades.clear();
  g_tasksMngr.clear();
  g_logevents.clearLogEvents();
  g_events.clearEvents();
  g_menucmds.clear();
  g_vault.clear();
  g_xvars.clear();
  g_plugins.clear();

  UTIL_Log("Log file closed.");

  RETURN_META(MRES_IGNORED);
}

BOOL ClientConnect_Post( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ]  ){
  CPlayer* pPlayer = GET_PLAYER_POINTER(pEntity);
  if (!pPlayer->bot) {

    bool a = pPlayer->Connect(pszName,pszAddress);

    g_forwards.executeForwards(FF_ClientConnect , 1,pPlayer->index);

    if ( a )
    {
      CPlayer** aa = new CPlayer*(pPlayer);
      if ( aa ) g_auth.put( aa );
    }
    else
    {
      pPlayer->Authorize();
      g_forwards.executeForwards(FF_ClientAuthorized , 1, pPlayer->index );
    }
  }
  RETURN_META_VALUE(MRES_IGNORED, TRUE);
}

void ClientDisconnect( edict_t *pEntity ) {
  CPlayer *pPlayer = GET_PLAYER_POINTER(pEntity);
  if (pPlayer->ingame)  {
    g_forwards.executeForwards(FF_ClientDisconnect , 1,pPlayer->index);
    pPlayer->Disconnect();
    --g_players_num;
  }
  RETURN_META(MRES_IGNORED);
}

void ClientPutInServer_Post( edict_t *pEntity ) {
  CPlayer *pPlayer = GET_PLAYER_POINTER(pEntity);
  if (!pPlayer->bot) {
    pPlayer->PutInServer();
    ++g_players_num;

    g_forwards.executeForwards(FF_ClientPutInServer , 1,pPlayer->index);

  }
  RETURN_META(MRES_IGNORED);
}

void ClientUserInfoChanged_Post( edict_t *pEntity, char *infobuffer ) {
  CPlayer *pPlayer = GET_PLAYER_POINTER(pEntity);

  g_forwards.executeForwards(FF_ClientInfoChanged , 1,pPlayer->index);

  const char* name = INFOKEY_VALUE(infobuffer,"name");

  // Emulate bot connection and putinserver
  if ( pPlayer->ingame )
  {
    pPlayer->name.set(name); // Make sure player have name up to date
  }
  else if ( pPlayer->IsBot() )
  {
    pPlayer->Connect( name ,"127.0.0.1"/*CVAR_GET_STRING("net_address")*/);

    g_forwards.executeForwards(FF_ClientConnect , 1,pPlayer->index);

    pPlayer->Authorize();
    g_forwards.executeForwards(FF_ClientAuthorized , 1, pPlayer->index );


    pPlayer->PutInServer();
    ++g_players_num;

    g_forwards.executeForwards(FF_ClientPutInServer , 1,pPlayer->index);
  }

  RETURN_META(MRES_IGNORED);
}

void ClientCommand( edict_t *pEntity ) {
  CPlayer *pPlayer = GET_PLAYER_POINTER(pEntity);
  META_RES result = MRES_IGNORED;
  cell ret = 0;
  int err;

#ifdef ENABLEEXEPTIONS
  try
  {
#endif
    /* because of PLUGIN_HANDLED_MAIN we must call function (client_command) manualy */

    CForwardMngr::iterator a = g_forwards.begin( FF_ClientCommand );

    while ( a )
    {
      if (  (*a).getPlugin()->isExecutable( (*a).getFunction()  )  )
      {

        if ((err = amx_Exec((*a).getPlugin()->getAMX(), &ret , (*a).getFunction(), 1, pPlayer->index)) != AMX_ERR_NONE)
          UTIL_Log("[AMXX] Run time error %d on line %ld (plugin \"%s\")",
          err,(*a).getPlugin()->getAMX()->curline,(*a).getPlugin()->getName() );

        if ( ret & 2 ) result = MRES_SUPERCEDE;
        if ( ret & 1 ) RETURN_META(MRES_SUPERCEDE);

      }

      ++a;
    }


#ifdef ENABLEEXEPTIONS
  }catch( ... )
  {
    UTIL_Log( "[AMXX] fatal error at commmand forward execution");
  }
#endif


  /* check for command and if needed also for first argument and call proper function */
  const char* cmd = CMD_ARGV(0);
  const char* arg = CMD_ARGV(1);

#ifdef ENABLEEXEPTIONS
  try{
#endif

    CmdMngr::iterator aa = g_commands.clcmdprefixbegin( cmd );
    if ( !aa ) aa = g_commands.clcmdbegin();

    while ( aa )
    {
      if ( (*aa).matchCommandLine( cmd , arg  ) &&
        (*aa).getPlugin()->isExecutable(  (*aa).getFunction() ) )
      {

        if ((err =amx_Exec((*aa).getPlugin()->getAMX(), &ret , (*aa).getFunction()   , 3, pPlayer->index, (*aa).getFlags(),(*aa).getId()  )) != AMX_ERR_NONE)
          UTIL_Log("[AMXX] Run time error %d on line %ld (plugin \"%s\")",
          err,(*aa).getPlugin()->getAMX()->curline,(*aa).getPlugin()->getName());

        if ( ret & 2 )  result = MRES_SUPERCEDE;
        if ( ret & 1 )  RETURN_META(MRES_SUPERCEDE);
      }

      ++aa;
    }

#ifdef ENABLEEXEPTIONS
  }catch( ... )
  {
    UTIL_Log( "[AMXX] fatal error at client commmand execution");
  }
#endif
  /* check menu commands */

  if (!strcmp(cmd,"menuselect"))
  {
    int pressed_key = atoi( arg ) - 1;
    int bit_key = (1<<pressed_key);

    if (pPlayer->keys & bit_key)
    {

      int menuid = pPlayer->menu;
      pPlayer->menu = 0;

#ifdef ENABLEEXEPTIONS
      try{
#endif
        MenuMngr::iterator a = g_menucmds.begin();

        while( a )
        {
          if ( (*a).matchCommand(  menuid , bit_key  ) && (*a).getPlugin()->isExecutable( (*a).getFunction() ) )
          {

            if ( ( err = amx_Exec((*a).getPlugin()->getAMX(), &ret ,(*a).getFunction() , 2, pPlayer->index,pressed_key)) != AMX_ERR_NONE)
              UTIL_Log("[AMXX] Run time error %d on line %ld (plugin \"%s\")",
              err,(*a).getPlugin()->getAMX()->curline,(*a).getPlugin()->getName());

            if ( ret & 2 ) result = MRES_SUPERCEDE;
            if ( ret & 1 ) RETURN_META(MRES_SUPERCEDE);
          }

          ++a;
        }

#ifdef ENABLEEXEPTIONS
      }
      catch( ... )
      {
        UTIL_Log( "[AMXX] fatal error at menu commmand execution");
      }
#endif
    }
  }
  /* check for PLUGIN_HANDLED_MAIN and block hl call if needed */
  RETURN_META( result );
}

void StartFrame_Post( void ) {

  if (g_auth_time < gpGlobals->time )
  {
  g_auth_time = gpGlobals->time + 0.7;

  CList<CPlayer*>::iterator a = g_auth.begin();

  while ( a )
  {
    const char* auth = GETPLAYERAUTHID( (*a)->pEdict );

    if ( (auth == 0) || (*auth == 0) ) {
      a.remove();
      continue;
    }

    if ( strcmp( auth, "STEAM_ID_PENDING" ) )
    {
      (*a)->Authorize();
      g_forwards.executeForwards(FF_ClientAuthorized , 1,(*a)->index);
      a.remove();
      continue;
    }

    ++a;
  }

  }




  if (g_task_time > gpGlobals->time)
    RETURN_META(MRES_IGNORED);

  g_task_time = gpGlobals->time + 0.1;

  for(CTaskMngr::iterator a = g_tasksMngr.begin(); a ; ++a)
  {
    CTaskMngr::CTask& task = *a;
    CPluginMngr::CPlugin* plugin = task.getPlugin();
    int err;

    if ( plugin->isExecutable( task.getFunction() ) )
    {
      if ( task.getParamLen() ) // call with arguments
      {
        cell amx_addr, *phys_addr;

        if (amx_Allot(plugin->getAMX(), task.getParamLen() , &amx_addr, &phys_addr) != AMX_ERR_NONE)
        {
          UTIL_Log("[AMXX] Failed to allocate AMX memory (task \"%d\") (plugin \"%s\")", task.getTaskId(),plugin->getName());
        }
        else
        {
          copy_amxmemory(phys_addr, task.getParam() , task.getParamLen() );

          if ((err = amx_Exec(plugin->getAMX(),NULL, task.getFunction() , 2, amx_addr, task.getTaskId() )) != AMX_ERR_NONE)
            UTIL_Log("[AMXX] Run time error %d on line %ld (task \"%d\") (plugin \"%s\")", err,plugin->getAMX()->curline,task.getTaskId(),plugin->getName());

          amx_Release(plugin->getAMX(), amx_addr);
        }
      }
      else // call without arguments
      {
        if ((err = amx_Exec(plugin->getAMX(),NULL, task.getFunction() ,1, task.getTaskId() )) != AMX_ERR_NONE)
          UTIL_Log("[AMXX] Run time error %d on line %ld (task \"%d\") (plugin \"%s\")", err,plugin->getAMX()->curline,task.getTaskId(),plugin->getName());
      }
    }
  }

  RETURN_META(MRES_IGNORED);
}

void MessageBegin_Post(int msg_dest, int msg_type, const float *pOrigin, edict_t *ed) {
  if (ed)
  {

    if (gmsgBattery==msg_type&&g_bmod_cstrike)
    {
      void* ptr = GET_PRIVATE(ed);
#ifdef __linux__
      int *z = (int*)ptr + 0x171;
#else
      int *z = (int*)ptr + 0x16C;
#endif
      int stop = ed->v.armorvalue;
      *z = stop;
      ed->v.armorvalue = stop;
    }

    mPlayerIndex = ENTINDEX(ed);
    mPlayer = GET_PLAYER_POINTER_I(mPlayerIndex);
  }
  else
  {
    mPlayerIndex = 0;
    mPlayer = 0;
  }
  if ( msg_type < 0 || msg_type >= MAX_REG_MSGS )
    msg_type = 0;

  mState = 0;
  function=modMsgs[msg_type];
  endfunction=modMsgsEnd[msg_type];
  g_events.parserInit(msg_type, &gpGlobals->time, mPlayer ,mPlayerIndex);
  RETURN_META(MRES_IGNORED);
}
void WriteByte_Post(int iValue) {
  g_events.parseValue(iValue);
  if (function) (*function)((void *)&iValue);
  RETURN_META(MRES_IGNORED);
}
void WriteChar_Post(int iValue) {
  g_events.parseValue(iValue);
  if (function) (*function)((void *)&iValue);
  RETURN_META(MRES_IGNORED);
}
void WriteShort_Post(int iValue) {
  g_events.parseValue(iValue);
  if (function) (*function)((void *)&iValue);
  RETURN_META(MRES_IGNORED);
}
void WriteLong_Post(int iValue) {
  g_events.parseValue(iValue);
  if (function) (*function)((void *)&iValue);
  RETURN_META(MRES_IGNORED);
}
void WriteAngle_Post(float flValue) {
  g_events.parseValue(flValue);
  if (function) (*function)((void *)&flValue);
  RETURN_META(MRES_IGNORED);
}
void WriteCoord_Post(float flValue) {
  g_events.parseValue(flValue);
  if (function) (*function)((void *)&flValue);
  RETURN_META(MRES_IGNORED);
}
void WriteString_Post(const char *sz) {
  g_events.parseValue(sz);
  if (function) (*function)((void *)sz);
  RETURN_META(MRES_IGNORED);
}
void WriteEntity_Post(int iValue) {
  g_events.parseValue(iValue);
  if (function) (*function)((void *)&iValue);
  RETURN_META(MRES_IGNORED);
}
void MessageEnd_Post(void) {
  g_events.executeEvents();

#if 0 // ######### this is done by call above
  EventsMngr::iterator a = g_events.begin();
  int err;
#ifdef ENABLEEXEPTIONS
  try
  {
#endif

    while ( a )
    {

      if ((err = amx_Exec((*a).getPlugin()->getAMX(), NULL ,  (*a).getFunction() , 1, mPlayerIndex  /*g_events.getArgInteger(0)*/ )) != AMX_ERR_NONE)
        UTIL_Log("[AMXX] Run time error %d on line %ld (plugin \"%s\")",err,(*a).getPlugin()->getAMX()->curline,(*a).getPlugin()->getName());


      ++a;

    }

#ifdef ENABLEEXEPTIONS
  }
  catch( ... )
  {
    UTIL_Log( "[AMXX] fatal error at event execution");
  }
#endif
#endif

  if (endfunction) (*endfunction)(NULL);
  RETURN_META(MRES_IGNORED);
}
const char *Cmd_Args( void ) {
  if (g_fakecmd.fake) RETURN_META_VALUE(MRES_SUPERCEDE, (g_fakecmd.argc>1)?g_fakecmd.args:NULL);
  RETURN_META_VALUE(MRES_IGNORED, NULL);
}

const char *Cmd_Argv( int argc ) {
  if (g_fakecmd.fake) RETURN_META_VALUE(MRES_SUPERCEDE, (argc<3)?g_fakecmd.argv[argc]:"");
  RETURN_META_VALUE(MRES_IGNORED, NULL);
}

int Cmd_Argc( void ) {
  if (g_fakecmd.fake) RETURN_META_VALUE(MRES_SUPERCEDE, g_fakecmd.argc );
  RETURN_META_VALUE(MRES_IGNORED, 0);
}

// Grenade has been thrown.
// Only here we may find out who is an owner.
void SetModel(edict_t *e, const char *m){
  if(e->v.owner&&m[7]=='w'&&m[8]=='_'&&m[9]=='h')
    g_grenades.put( e , 1.75, 4, GET_PLAYER_POINTER(e->v.owner) );
  RETURN_META(MRES_IGNORED);
}

// Save at what part of body a player is aiming
void TraceLine_Post(const float *v1, const float *v2, int fNoMonsters, edict_t *e, TraceResult *ptr) {
  if ( e && ( e->v.flags & (FL_CLIENT | FL_FAKECLIENT) ) ) {
    CPlayer* pPlayer = GET_PLAYER_POINTER(e);
    if (ptr->pHit&&(ptr->pHit->v.flags& (FL_CLIENT | FL_FAKECLIENT) ))
      pPlayer->aiming = ptr->iHitgroup;
    pPlayer->lastTrace = pPlayer->thisTrace;
    pPlayer->thisTrace = ptr->vecEndPos;
  }
  RETURN_META(MRES_IGNORED);
}

void AlertMessage_Post(ALERT_TYPE atype, char *szFmt, ...) {

  if ( atype != at_logged ) RETURN_META(MRES_IGNORED);

  /*  There are also more messages but we want only logs
  at_notice,
  at_console,   // same as at_notice, but forces a ConPrintf, not a message box
  at_aiconsole, // same as at_console, but only shown if developer level is 2!
  at_warning,
  at_error,
  at_logged   // Server print to console ( only in multiplayer games ).
  */

  if ( g_logevents.logEventsExist() )
  {
    va_list logArgPtr;
    va_start ( logArgPtr , szFmt );
    g_logevents.setLogString( szFmt , logArgPtr );
    va_end ( logArgPtr );
    g_logevents.parseLogString( );
    g_logevents.executeLogEvents( );


#if 0 // ######### this is done by call above
    LogEventsMngr::iterator a = g_logevents.begin();
    int err;
#ifdef ENABLEEXEPTIONS
    try
    {
#endif
      while ( a )
      {
        if ((err = amx_Exec((*a).getPlugin()->getAMX(), NULL , (*a).getFunction() , 1,mPlayerIndex)) != AMX_ERR_NONE)
          UTIL_Log("[AMXX] Run time error %d on line %ld (plugin \"%s\")",err,(*a).getPlugin()->getAMX()->curline,(*a).getPlugin()->getName());

        ++a;

      }

#ifdef ENABLEEXEPTIONS
    }
    catch( ... )
    {
      UTIL_Log( "[AMXX] fatal error at log event execution");
    }
#endif
#endif

    g_forwards.executeForwards(FF_PluginLog);
  }
  else if (g_forwards.forwardsExist( FF_PluginLog ))
  {
    va_list logArgPtr;
    va_start ( logArgPtr , szFmt );
    g_logevents.setLogString( szFmt , logArgPtr );
    va_end ( logArgPtr );
    g_logevents.parseLogString( );
    g_forwards.executeForwards(FF_PluginLog);
  }
  RETURN_META(MRES_IGNORED);
}

C_DLLEXPORT int Meta_Query(char *ifvers, plugin_info_t **pPlugInfo, mutil_funcs_t *pMetaUtilFuncs) {
  gpMetaUtilFuncs=pMetaUtilFuncs;
  *pPlugInfo=&Plugin_info;
  if(strcmp(ifvers, Plugin_info.ifvers)) {
    int mmajor=0, mminor=0, pmajor=0, pminor=0;
    LOG_MESSAGE(PLID, "WARNING: meta-interface version mismatch; requested=%s ours=%s", Plugin_info.logtag, ifvers);
    sscanf(ifvers, "%d:%d", &mmajor, &mminor);
    sscanf(META_INTERFACE_VERSION, "%d:%d", &pmajor, &pminor);
    if(pmajor > mmajor || (pmajor==mmajor && pminor > mminor)) {
      LOG_ERROR(PLID, "metamod version is too old for this plugin; update metamod");
      return(FALSE);
    }
    else if(pmajor < mmajor) {
      LOG_ERROR(PLID, "metamod version is incompatible with this plugin; please find a newer version of this plugin");
      return(FALSE);
    }
    else if(pmajor==mmajor && pminor < mminor)
      LOG_MESSAGE(PLID, "WARNING: metamod version is newer than expected; consider finding a newer version of this plugin");
    else
      LOG_ERROR(PLID, "unexpected version comparison; metavers=%s, mmajor=%d, mminor=%d; plugvers=%s, pmajor=%d, pminor=%d", ifvers, mmajor, mminor, META_INTERFACE_VERSION, pmajor, pminor);
  }
  return(TRUE);
}

static META_FUNCTIONS gMetaFunctionTable;
C_DLLEXPORT int Meta_Attach(PLUG_LOADTIME now, META_FUNCTIONS *pFunctionTable, meta_globals_t *pMGlobals, gamedll_funcs_t *pGamedllFuncs) {
  if(now > Plugin_info.loadable) {
    LOG_ERROR(PLID, "Can't load plugin right now");
    return(FALSE);
  }
  gpMetaGlobals=pMGlobals;
  gMetaFunctionTable.pfnGetEntityAPI2 = GetEntityAPI2;
  gMetaFunctionTable.pfnGetEntityAPI2_Post = GetEntityAPI2_Post;
  gMetaFunctionTable.pfnGetEngineFunctions = GetEngineFunctions;
  gMetaFunctionTable.pfnGetEngineFunctions_Post = GetEngineFunctions_Post;
  memcpy(pFunctionTable, &gMetaFunctionTable, sizeof(META_FUNCTIONS));
  gpGamedllFuncs=pGamedllFuncs;
  CVAR_REGISTER (&init_amx_version);
  CVAR_REGISTER (&init_amxmodx_version);
  amx_version = CVAR_GET_POINTER(init_amx_version.name  );
  amxmodx_version = CVAR_GET_POINTER(init_amxmodx_version.name);
  REG_SVR_COMMAND("amxx",amx_command);

  char gameDir[512];
  GET_GAME_DIR(gameDir);
  char *a = gameDir;
  int i = 0;
  while ( gameDir[i] )
    if (gameDir[i++] == '/')
      a = &gameDir[i];
  g_mod_name.set(a);

  //  ###### Now attach metamod modules
  attachMetaModModules( get_localinfo("amxx_modules" ,
    "addons/amxx/modules.ini" ) );

  return(TRUE);
}

C_DLLEXPORT int Meta_Detach(PLUG_LOADTIME now, PL_UNLOAD_REASON reason) {
  if(now > Plugin_info.unloadable && reason != PNL_CMD_FORCED) {
    LOG_ERROR(PLID, "Can't unload plugin right now");
    return(FALSE);
  }
  g_auth.clear();
  g_forwards.clear();
  g_commands.clear();
  g_forcemodels.clear();
  g_forcesounds.clear();
  g_forcegeneric.clear();
  g_grenades.clear();
  g_tasksMngr.clear();
  g_logevents.clearLogEvents();
  g_events.clearEvents();
  g_menucmds.clear();
  g_vault.clear();
  g_xvars.clear();
  g_plugins.clear();
  g_cvars.clear();

  dettachModules();

  //  ###### Now dettach metamod modules
  dettachMetaModModules( get_localinfo("amxx_modules" ,
    "addons/amxx/modules.ini" ) );

  return(TRUE);
}

C_DLLEXPORT void WINAPI GiveFnptrsToDll( enginefuncs_t* pengfuncsFromEngine, globalvars_t *pGlobals ) {
  memcpy(&g_engfuncs, pengfuncsFromEngine, sizeof(enginefuncs_t));
  gpGlobals = pGlobals;
}

DLL_FUNCTIONS gFunctionTable;
C_DLLEXPORT int GetEntityAPI2( DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion ){
  gFunctionTable.pfnSpawn = Spawn;
  gFunctionTable.pfnClientCommand = ClientCommand;
  gFunctionTable.pfnServerDeactivate = ServerDeactivate;
  gFunctionTable.pfnClientDisconnect = ClientDisconnect;
  gFunctionTable.pfnInconsistentFile = InconsistentFile;
  gFunctionTable.pfnServerActivate = ServerActivate;

  if(*interfaceVersion!=INTERFACE_VERSION) {
    LOG_ERROR(PLID, "GetEntityAPI2 version mismatch; requested=%d ours=%d", *interfaceVersion, INTERFACE_VERSION);
    *interfaceVersion = INTERFACE_VERSION;
    return(FALSE);
  }
  memcpy( pFunctionTable, &gFunctionTable, sizeof( DLL_FUNCTIONS ) );
  return(TRUE);
}

DLL_FUNCTIONS gFunctionTable_Post;
C_DLLEXPORT int GetEntityAPI2_Post( DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion ) {
  gFunctionTable_Post.pfnClientPutInServer = ClientPutInServer_Post;
  gFunctionTable_Post.pfnClientUserInfoChanged = ClientUserInfoChanged_Post;
  gFunctionTable_Post.pfnServerActivate = ServerActivate_Post;
  gFunctionTable_Post.pfnClientConnect = ClientConnect_Post;
  gFunctionTable_Post.pfnStartFrame = StartFrame_Post;
  gFunctionTable_Post.pfnServerDeactivate = ServerDeactivate_Post;

  if(*interfaceVersion!=INTERFACE_VERSION) {
    LOG_ERROR(PLID, "GetEntityAPI2_Post version mismatch; requested=%d ours=%d", *interfaceVersion, INTERFACE_VERSION);
    *interfaceVersion = INTERFACE_VERSION;
    return(FALSE);
  }
  memcpy( pFunctionTable, &gFunctionTable_Post, sizeof( DLL_FUNCTIONS ) );
  return(TRUE);
}

enginefuncs_t meta_engfuncs;
C_DLLEXPORT int GetEngineFunctions(enginefuncs_t *pengfuncsFromEngine, int *interfaceVersion ) {

  if ( stricmp(g_mod_name.str(),"cstrike") == 0 )
  {
    meta_engfuncs.pfnSetModel = SetModel;
    g_bmod_cstrike = true;
  }
  else
  {
    g_bmod_cstrike = false;
    g_bmod_dod = !stricmp(g_mod_name.str(),"dod");
  }

  meta_engfuncs.pfnCmd_Argc = Cmd_Argc;
  meta_engfuncs.pfnCmd_Argv = Cmd_Argv;
  meta_engfuncs.pfnCmd_Args = Cmd_Args;
  meta_engfuncs.pfnPrecacheModel = PrecacheModel;
  meta_engfuncs.pfnPrecacheSound = PrecacheSound;

  if(*interfaceVersion!=ENGINE_INTERFACE_VERSION) {
    LOG_ERROR(PLID, "GetEngineFunctions version mismatch; requested=%d ours=%d", *interfaceVersion, ENGINE_INTERFACE_VERSION);
    *interfaceVersion = ENGINE_INTERFACE_VERSION;
    return(FALSE);
  }
  memcpy(pengfuncsFromEngine, &meta_engfuncs, sizeof(enginefuncs_t));
  return(TRUE);
}

enginefuncs_t meta_engfuncs_post;
C_DLLEXPORT int GetEngineFunctions_Post(enginefuncs_t *pengfuncsFromEngine, int *interfaceVersion ) {
  meta_engfuncs_post.pfnTraceLine = TraceLine_Post;
  meta_engfuncs_post.pfnMessageBegin = MessageBegin_Post;
  meta_engfuncs_post.pfnMessageEnd = MessageEnd_Post;
  meta_engfuncs_post.pfnWriteByte = WriteByte_Post;
  meta_engfuncs_post.pfnWriteChar = WriteChar_Post;
  meta_engfuncs_post.pfnWriteShort = WriteShort_Post;
  meta_engfuncs_post.pfnWriteLong = WriteLong_Post;
  meta_engfuncs_post.pfnWriteAngle = WriteAngle_Post;
  meta_engfuncs_post.pfnWriteCoord = WriteCoord_Post;
  meta_engfuncs_post.pfnWriteString = WriteString_Post;
  meta_engfuncs_post.pfnWriteEntity =  WriteEntity_Post;
  meta_engfuncs_post.pfnAlertMessage =  AlertMessage_Post;
  meta_engfuncs_post.pfnRegUserMsg = RegUserMsg_Post;


  if(*interfaceVersion!=ENGINE_INTERFACE_VERSION) {
    LOG_ERROR(PLID, "GetEngineFunctions_Post version mismatch; requested=%d ours=%d", *interfaceVersion, ENGINE_INTERFACE_VERSION);
    *interfaceVersion = ENGINE_INTERFACE_VERSION;
    return(FALSE);
  }
  memcpy(pengfuncsFromEngine, &meta_engfuncs_post, sizeof(enginefuncs_t));
  return(TRUE);
}
