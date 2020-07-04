/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <compat/strl.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <lists/string_list.h>
#include <retro_miscellaneous.h>
#include <configuration.h>
#include <verbosity.h>
#include <features/features_cpu.h>

#include <libretro.h>
#include "../wifi_driver.h"
#include "../../retroarch.h"
//#include "../../lakka.h"
#ifdef HAVE_GFX_WIDGETS
#include "../../gfx/gfx_widgets.h"
#endif

#define CACHE_INTERVAL_US 2000000

typedef struct {
    retro_time_t last_cache_update;
    struct string_list* ssids;
    struct string_list* active_ssids;
} netcfg_t;

static void *netcfg_init(void)
{
    netcfg_t *netcfg = (netcfg_t*)calloc(1, sizeof(netcfg_t));
    netcfg->ssids = string_list_new();
    netcfg->active_ssids = string_list_new();
    netcfg->last_cache_update = 0;

    return netcfg;
}

static void netcfg_free(void *data)
{
    netcfg_t *netcfg = (netcfg_t*)data;
    if (netcfg)
    {
        string_list_free(netcfg->ssids);
        string_list_free(netcfg->active_ssids);
        free(netcfg);
    }
}

static bool netcfg_start(void *data)
{
   (void)data;
   return true;
}

static void netcfg_stop(void *data)
{
   (void)data;
}

static void netcfg_scan(void *data)
{
   (void)data;
   FILE *cmd_file = NULL;
   int exit_status;
   char line[512];

   cmd_file = popen("netcfg scan", "r");
   while (fgets(line, 512, cmd_file))
   {
   }
   exit_status = pclose(cmd_file);

   if (exit_status == 0)
   {
       runloop_msg_queue_push(msg_hash_to_str(MSG_WIFI_SCAN_COMPLETE),
             1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
             MESSAGE_QUEUE_CATEGORY_INFO);
   }
   else
   {
       runloop_msg_queue_push("WiFi scan error",
             1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
             MESSAGE_QUEUE_CATEGORY_ERROR);
   }
}

static void netcfg_get_ssids(void* data, struct string_list* ssids)
{
   netcfg_t *netcfg = (netcfg_t*)data;
   unsigned i;
   union string_list_elem_attr attr;
   FILE *cmd_file = NULL;
   char line[512];

   attr.i = RARCH_FILETYPE_UNSET;

   if (netcfg->ssids)
      string_list_free(netcfg->ssids);
   netcfg->ssids = string_list_new();

   cmd_file = popen("netcfg ssids", "r");
   while (fgets(line, 512, cmd_file))
   {
      string_list_append(netcfg->ssids, string_trim_whitespace(line), attr);
      string_list_append(ssids, string_trim_whitespace(line), attr);
   }
}

static bool netcfg_ssid_is_online(void* data, unsigned i)
{
   netcfg_t *netcfg = (netcfg_t*)data;
   char line[512];
   FILE* cmd_file = NULL;
   union string_list_elem_attr attr;

   attr.i = RARCH_FILETYPE_UNSET;
   retro_time_t curTs = cpu_features_get_time_usec();
   if (curTs > netcfg->last_cache_update + CACHE_INTERVAL_US)
   {
      netcfg->last_cache_update = curTs;

      if (netcfg->active_ssids)
         string_list_free(netcfg->active_ssids);
      netcfg->active_ssids = string_list_new();

      cmd_file = popen("netcfg online", "r");

      while (fgets(line, 512, cmd_file))
      {
          string_list_append(netcfg->active_ssids, string_trim_whitespace(line), attr);
      }
      pclose(cmd_file);
   }

   return !!string_list_find_elem(netcfg->active_ssids, netcfg->ssids->elems[i].data);
}

static bool netcfg_connect_ssid(void* data, unsigned i, const char* passphrase)
{
   netcfg_t *netcfg = (netcfg_t*)data;
   char ln[512] = {0};
   FILE *cmd_file = NULL;
   char command[256];
   const char* ssid = netcfg->ssids->elems[i].data;

   snprintf(command, sizeof(command), "netcfg connect \"%s\" \"%s\"", ssid, passphrase);
   cmd_file = popen(command, "r");

   while (fgets(ln, sizeof(ln), cmd_file))
   {
#ifdef HAVE_GFX_WIDGETS
      if (!gfx_widgets_ready())
#endif
         runloop_msg_queue_push(ln, 1, 180, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT,
               MESSAGE_QUEUE_CATEGORY_INFO);
   }
   pclose(cmd_file);

   return true;
}

static void netcfg_tether_start_stop(void* data, bool start, char* configfile)
{
    (void)data;
    (void)start;
    (void)configfile;
}

wifi_driver_t wifi_netcfg = {
   netcfg_init,
   netcfg_free,
   netcfg_start,
   netcfg_stop,
   netcfg_scan,
   netcfg_get_ssids,
   netcfg_ssid_is_online,
   netcfg_connect_ssid,
   netcfg_tether_start_stop,
   "netcfg",
};
