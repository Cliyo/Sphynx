#ifndef OfflineDB_h
#define OfflineDB_h

#include <Preferences.h>

class OfflineDatabaseClass {
  private:
    const char* NAMESPACE = "access_db";
    const char* TOKEN_KEY = "auth_token";
  public:
    void begin();
    void addAuthorizedTag(String tagId);
    void removeAuthorizedTag(String tagID);
    bool isTagAuthorized(String tagID);
    void setOfflineToken(String token);
    bool validateToken(String token);
    void clearAll();
};
extern OfflineDatabaseClass OfflineDB;
#endif
