/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "../../Framework/Odbc/OdbcDatabase.h"
#include "../../Framework/Plugins/PluginInitialization.h"
#include "../../Framework/Plugins/StorageBackend.h"
#include "../../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

#include <EmbeddedResources.h>  // Autogenerated file

#include <Logging.h>


namespace OrthancDatabases
{
  class OdbcStorageArea : public StorageBackend
  {
  protected:
    virtual bool HasReadRange() const ORTHANC_OVERRIDE
    {
      // Read range is only available in native PostgreSQL/MySQL plugins
      return false;
    }

  public:
    OdbcStorageArea(unsigned int maxConnectionRetries,
                    unsigned int connectionRetryInterval,
                    const std::string& connectionString) :
      StorageBackend(OdbcDatabase::CreateDatabaseFactory(
                       maxConnectionRetries, connectionRetryInterval, connectionString, false),
                     maxConnectionRetries)
    {
      {
        AccessorBase accessor(*this);
        OdbcDatabase& db = dynamic_cast<OdbcDatabase&>(accessor.GetManager().GetDatabase());
        
        if (!db.DoesTableExist("storagearea"))
        {
          std::string sql;
          Orthanc::EmbeddedResources::GetFileResource(sql, Orthanc::EmbeddedResources::ODBC_PREPARE_STORAGE);
          
          switch (db.GetDialect())
          {
            case Dialect_SQLite:
              boost::replace_all(sql, "${BINARY}", "BLOB");
              break;
              
            case Dialect_PostgreSQL:
              boost::replace_all(sql, "${BINARY}", "BYTEA");
              break;
              
            case Dialect_MySQL:
              boost::replace_all(sql, "${BINARY}", "LONGBLOB");
              break;
              
            case Dialect_MSSQL:
              boost::replace_all(sql, "${BINARY}", "VARBINARY(MAX)");
              break;
              
            default:
              throw Orthanc::OrthancException(Orthanc::ErrorCode_NotImplemented);
          }

          {
            DatabaseManager::Transaction t(accessor.GetManager(), TransactionType_ReadWrite);
            db.ExecuteMultiLines(sql);
            t.Commit();
          }
        }
      }
    }
  };
}



#if defined(_WIN32)
#  include <windows.h>
#else
#  include <ltdl.h>
#  include <libltdl/lt_dlloader.h>
#endif


static const char* const KEY_ODBC = "Odbc";


extern "C"
{
#if !defined(_WIN32)
  extern lt_dlvtable *dlopen_LTX_get_vtable(lt_user_data loader_data);
#endif

  
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* context)
  {
    if (!OrthancDatabases::InitializePlugin(context, "ODBC", false))
    {
      return -1;
    }

#if !defined(_WIN32)
    lt_dlinit();
    
    /**
     * The following call is necessary for "libltdl" to access the
     * "dlopen()" primitives if statically linking. Otherwise, only the
     * "preopen" primitives are available.
     **/
    lt_dlloader_add(dlopen_LTX_get_vtable(NULL));
#endif

    OrthancPlugins::OrthancConfiguration configuration;

    if (!configuration.IsSection(KEY_ODBC))
    {
      LOG(WARNING) << "No available configuration for the ODBC storage area plugin";
      return 0;
    }

    OrthancPlugins::OrthancConfiguration odbc;
    configuration.GetSection(odbc, KEY_ODBC);

    bool enable;
    if (!odbc.LookupBooleanValue(enable, "EnableStorage") ||
        !enable)
    {
      LOG(WARNING) << "The ODBC storage area is currently disabled, set \"EnableStorage\" "
                   << "to \"true\" in the \"" << KEY_ODBC << "\" section of the configuration file of Orthanc";
      return 0;
    }

    OrthancDatabases::OdbcEnvironment::GlobalInitialization();

    try
    {
      const std::string connectionString = odbc.GetStringValue("StorageConnectionString", "");
      const unsigned int maxConnectionRetries = odbc.GetUnsignedIntegerValue("MaxConnectionRetries", 10);
      const unsigned int connectionRetryInterval = odbc.GetUnsignedIntegerValue("ConnectionRetryInterval", 5);

      if (connectionString.empty())
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange,
                                        "No connection string provided for the ODBC storage area");
      }

      OrthancDatabases::StorageBackend::Register(
        context, new OrthancDatabases::OdbcStorageArea(
          maxConnectionRetries, connectionRetryInterval, connectionString));
    }
    catch (Orthanc::OrthancException& e)
    {
      LOG(ERROR) << e.What();
      return -1;
    }
    catch (...)
    {
      LOG(ERROR) << "Native exception while initializing the plugin";
      return -1;
    }

    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    LOG(WARNING) << "ODBC storage area is finalizing";
    OrthancDatabases::StorageBackend::Finalize();
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "odbc-storage";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return ORTHANC_PLUGIN_VERSION;
  }
}
