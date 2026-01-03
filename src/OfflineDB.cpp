#include "OfflineDB.h"

Preferences prefs;

void OfflineDatabaseClass::begin() {
  prefs.begin(NAMESPACE, false); 
}

void OfflineDatabaseClass::addAuthorizedTag(String tagID) {
  tagID.replace(" ", ""); 
  if(tagID.length() <= 15) {
    prefs.putBool(tagID.c_str(), true);
  }
}

void OfflineDatabaseClass::removeAuthorizedTag(String tagID) {
  tagID.replace(" ", "");
  prefs.remove(tagID.c_str());
}

bool OfflineDatabaseClass::isTagAuthorized(String tagID) {
  tagID.replace(" ", "");
  return prefs.getBool(tagID.c_str(), false);
}

void OfflineDatabaseClass::setOfflineToken(String token) {
  prefs.putString(TOKEN_KEY, token);
}

bool OfflineDatabaseClass::validateToken(String token) {
  String storedToken = prefs.getString(TOKEN_KEY, "default_token"); 
  return (token == storedToken);
}
    
void OfflineDatabaseClass::clearAll() {
  prefs.clear();
}

OfflineDatabaseClass OfflineDB;
