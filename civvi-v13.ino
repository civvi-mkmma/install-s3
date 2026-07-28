#include <base64.h>
#include <LittleFS.h>
#include <esp_system.h>
#include <SPI.h>
#include <esp_now.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <map>
#include <sys/time.h>
#include <time.h>
#include <vector>

// forward declarations
void sendNoCacheHeaders();


// Section des variables globales de configuration du module
String monNomReseau = "civvi-";            
String monMotDePasse = ""; 
String monMessageReboot = "";  
String monNomUtilisateur = ""; 
String monNomUtilisateurP2P = ""; 


struct SalonMessage {
  String auteur;
  String texte;
  unsigned long timestamp;
};
std::vector<SalonMessage> salonMessages;
int fonctionnementMode = 0; 

IPAddress apIP(192, 168, 4, 1);
WebServer server(80);


String maPensee = "";
String monID = "";
uint32_t monMsgID = 0;

#define MAX_MESSAGES 144
#define TIMEOUT_MS 180000    
#define MAX_PAYLOAD_SIZE 120 
#define MAX_CHUNKS 2000        

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t peerInfo;

struct Message {
  String auteur;
  String nomReseau;
  String texte;
  int rssi;
  unsigned long dernierContact;
  int contrast;
};

struct MessageIncomplet {
  String nomReseau;
  int total;
  int recu;
  int rssi;
  std::vector<String> morceaux;
  unsigned long dernierUpdate;
  int contrast;
};

struct VoteNotification {
  String voteur;
  String emoticon;
  unsigned long timeReceived;
};

std::vector<Message> messages;
std::map<String, MessageIncomplet> messagesEnAttente;
std::vector<VoteNotification> voteNotifications;
std::vector<String> favorites;
std::vector<String> banned;
struct CVRequest {
  String targetMac;
  String requesterMac;
};
std::vector<CVRequest> cvRequestsQueue;
int monContraste = 0;
std::map<String, std::map<String, int>> votesMap; 
std::map<String, MessageIncomplet> cvsEnAttente;
std::map<String, String> cvsStockes;
std::vector<CVRequest> docRequestsQueue;
std::map<String, MessageIncomplet> docsEnAttente;
std::map<String, String> docsStockes;
int rediffuserDernier = 0;
bool salonHostConnected = false;
String salonHostIP = "";
File sdUploadFile;



// Neighbor mini-sites and SD cards chunked assemblies






unsigned long dernierEnvoi = 0;
volatile bool resauvegarder = false;


const int MAX_GRAFFITIS = 50;
const char *GRAFFITIS_FILE = "/messages.txt";
const char *PENSEES_FILE = "/pensees.txt";


const char *CONFIG_FILE = "/config.txt";


void ajouterAuJournal(String auteur, String nomReseau, String texte);
void ajouterAHistory(String texte);
String obtenirTempsFormate();

// CHAPITRE 1 / Paragraphe 1.4 : Utilitaires d'adressage et filtres
// Vérifie si un module (défini par sa MAC) fait partie de la liste noire (banned) pour l'ignorer
bool estBanni(String mac) {
  mac.toLowerCase();
  mac.trim();
  for (String m : banned) {
    if (m == mac) return true;
  }
  return false;
}

// Vérifie si un module (défini par sa MAC) fait partie de la liste des favoris (favorites)
bool estFavori(String mac) {
  mac.toLowerCase();
  mac.trim();
  for (String m : favorites) {
    if (m == mac) return true;
  }
  return false;
}

// CHAPITRE 2 : GESTION DES LISTES ET PROFILS
// Paragraphe 2.1 : chargerListes (Lecture des favoris/bannissements depuis LittleFS)
// Charge les listes de favoris et de bannis depuis les fichiers texte de LittleFS
void chargerListes() {
  favorites.clear();
  if (LittleFS.exists("/favorites.txt")) {
    File f = LittleFS.open("/favorites.txt", "r");
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      line.toLowerCase();
      if (line.length() > 0) {
        favorites.push_back(line);
      }
    }
    f.close();
  }
  
  banned.clear();
  if (LittleFS.exists("/banned.txt")) {
    File f = LittleFS.open("/banned.txt", "r");
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      line.toLowerCase();
      if (line.length() > 0) {
        banned.push_back(line);
      }
    }
    f.close();
  }
}

// Paragraphe 2.2 : API Profils (avatar et centres d'intérêt)
// Gère la récupération du fichier profil.json contenant l'avatar structuré
void handleGetProfile() {
  if (LittleFS.exists("/profile.json")) {
    File f = LittleFS.open("/profile.json", "r");
    server.streamFile(f, "application/json");
    f.close();
  } else {
    server.send(200, "application/json", "{}");
  }
}

// Gère la mise à jour (enregistrement) du fichier profil.json reçu depuis le client
void handlePostProfile() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    File f = LittleFS.open("/profile.json", "w");
    if (f) {
      f.print(body);
      f.close();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(500, "text/plain", "Error writing profile");
    }
  } else {
    server.send(400, "text/plain", "Body missing");
  }
}


// --- SD CARD LOGIC ---










// --- MINI SITE LOGIC ---
// Paragraphe 2.3 : [FONCTION LEGACY/INACTIVE] handleSiteUpload
// Ancienne fonction d'importation de mini-sites (non utilisée dans la version active)
void handleSiteUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    String path;
    if (filename == "/index.html" || filename.endsWith("/index.html")) {
      path = "/site/index.html";
    } else {
      path = "/site" + filename;
    }
    sdUploadFile = LittleFS.open(path, "w");
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (sdUploadFile) sdUploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (sdUploadFile) sdUploadFile.close();
  }
}

// Paragraphe 2.3 : [FONCTION LEGACY/INACTIVE] handleMiniSitePath
// Gère le routage pour l'ancienne interface des mini-sites (non utilisée dans la version active)
bool handleMiniSitePath(String path) {
  if (path.endsWith("/")) path += "index.html";
  String contentType = "text/plain";
  if (path.endsWith(".html")) contentType = "text/html";
  else if (path.endsWith(".css")) contentType = "text/css";
  else if (path.endsWith(".js")) contentType = "application/javascript";
  else if (path.endsWith(".png")) contentType = "image/png";
  else if (path.endsWith(".jpg")) contentType = "image/jpeg";
  
  String fsPath = "/site" + path;
  if (LittleFS.exists(fsPath)) {
    File file = LittleFS.open(fsPath, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}



// CHAPITRE 3 : INITIALISATION ET CONFIGURATION SYSTÈME
// Paragraphe 3.1 : chargerConfig (Chargement de la configuration réseau/mode)
// Charge le fichier de configuration de l'ESP contenant SSID, mot de passe, reboot message, etc.
void chargerConfig() {
  if (LittleFS.exists(CONFIG_FILE)) {
    File f = LittleFS.open(CONFIG_FILE, "r");
    if (f) {
      String ssid = f.readStringUntil('\n');
      String pwd = f.readStringUntil('\n');
      String rebootMsg = f.readStringUntil('\n');
      String username = f.readStringUntil('\n');
      String modeStr = f.readStringUntil('\n');
      ssid.trim();
      pwd.trim();
      rebootMsg.trim();
      username.trim();
      modeStr.trim();
      if (ssid.length() > 0) {
        monNomReseau = ssid;
        monMotDePasse = pwd;
      }
      if (rebootMsg.length() > 0) {
        monMessageReboot = rebootMsg;
      }
      if (username.length() > 0) {
        monNomUtilisateur = username;
      } else {
        monNomUtilisateur = monNomReseau;
      }
      if (modeStr.length() > 0) {
        fonctionnementMode = modeStr.toInt();
      } else {
        fonctionnementMode = 0;
      }
      String rediffStr = f.readStringUntil('\n');
      rediffStr.trim();
      if (rediffStr.length() > 0) {
        rediffuserDernier = rediffStr.toInt();
      } else {
        rediffuserDernier = 0;
      }
      String savedP2PUsername = f.readStringUntil('\n');
      savedP2PUsername.trim();
      if (savedP2PUsername.length() > 0) {
        monNomUtilisateurP2P = savedP2PUsername;
      } else {
        monNomUtilisateurP2P = username;
      }
      f.close();

      esp_reset_reason_t reason = esp_reset_reason();
      bool forceModeChange = false;
      int forcedMode = 0;
      if (LittleFS.exists("/mode_change.txt")) {
        File fTemp = LittleFS.open("/mode_change.txt", "r");
        if (fTemp) {
          String fm = fTemp.readStringUntil('\n');
          fm.trim();
          if (fm.length() > 0) {
            forcedMode = fm.toInt();
            forceModeChange = true;
          }
          fTemp.close();
        }
        LittleFS.remove("/mode_change.txt");
      }

      if (forceModeChange) {
        fonctionnementMode = forcedMode;
        if (fonctionnementMode == 0 && monNomUtilisateurP2P.length() > 0) {
          monNomUtilisateur = monNomUtilisateurP2P;
        }
        File fWrite = LittleFS.open(CONFIG_FILE, "w");
        if (fWrite) {
          fWrite.println(monNomReseau);
          fWrite.println(monMotDePasse);
          fWrite.println(monMessageReboot);
          fWrite.println(monNomUtilisateur);
          fWrite.println(String(fonctionnementMode));
          fWrite.println(String(rediffuserDernier));
          fWrite.println(monNomUtilisateurP2P);
          fWrite.close();
        }
      } else if (reason != ESP_RST_SW && fonctionnementMode != 0) {
        fonctionnementMode = 0;
        if (monNomUtilisateurP2P.length() > 0) {
          monNomUtilisateur = monNomUtilisateurP2P;
        }
        File fWrite = LittleFS.open(CONFIG_FILE, "w");
        if (fWrite) {
          fWrite.println(monNomReseau);
          fWrite.println(monMotDePasse);
          fWrite.println(monMessageReboot);
          fWrite.println(monNomUtilisateur);
          fWrite.println("0");
          fWrite.println(String(rediffuserDernier));
          fWrite.println(monNomUtilisateurP2P);
          fWrite.close();
        }
      }
    }
  }
  if (monNomUtilisateur.length() == 0) {
    monNomUtilisateur = monNomReseau;
  }
  if (monNomUtilisateurP2P.length() == 0) {
    monNomUtilisateurP2P = monNomUtilisateur;
  }
}

// Paragraphe 3.2 : initFS (Initialisation du système de fichiers LittleFS)
// Initialise le système de fichiers LittleFS et vérifie sa disponibilité
void initFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("Erreur: Impossible de monter LittleFS");
    return;
  }
  Serial.println("LittleFS monté avec succès");

  if (!LittleFS.exists(GRAFFITIS_FILE)) {
    File file = LittleFS.open(GRAFFITIS_FILE, "w");
    if (file) {
      file.print("");
      file.close();
      Serial.println("Fichier de graffitis créé.");
    }
  }
}

// Paragraphe 3.3 : chargerPensees / sauvegarderPensees (Gestion des messages stockés)
// Enregistre les pensées/messages du réseau local dans LittleFS (pensees.txt)
void sauvegarderPensees() {
  File f = LittleFS.open(PENSEES_FILE, FILE_WRITE);
  if (!f)
    return;
  
  
  f.println(monID + "|" + monNomReseau + "|" + maPensee);
  f.close();
}

// Charge les pensées/messages du réseau local depuis LittleFS (pensees.txt) au démarrage
void chargerPensees() {
  if (monMessageReboot.length() > 0) {
    maPensee = monMessageReboot;
  }

  if (!LittleFS.exists(PENSEES_FILE))
    return;
  File f = LittleFS.open(PENSEES_FILE, FILE_READ);
  while (f.available()) {
    String ligne = f.readStringUntil('\n');
    ligne.trim();
    int s1 = ligne.indexOf('|');
    int s2 = ligne.indexOf('|', s1 + 1);
    if (s1 > 0 && s2 > 0) {
      String auteur = ligne.substring(0, s1);
      String nom = ligne.substring(s1 + 1, s2);
      String texte = ligne.substring(s2 + 1);
      if (auteur == monID) {
        
        if (monMessageReboot.length() == 0) {
          maPensee = texte;
        }
      }
      
      
    }
  }
  f.close();
}

// CHAPITRE 4 : PROTOCOLE RADIO ESP-NOW (P2P)
// Paragraphe 4.1 : Fonctions d'envoi radio
// Transmet l'avatar structuré (CV) d'un module par morceaux radio (ESP-NOW)
void envoyerCV(String targetMac, String requesterMac) {
  String cvText = "";
  if (LittleFS.exists("/profile.json")) {
    File file = LittleFS.open("/profile.json", "r");
    cvText = file.readString();
    file.close();
  }
  if (cvText.length() == 0 || !cvText.startsWith("{")) {
    cvText = "{}";
  }

  std::vector<String> morceaux;
  String texteRestant = cvText;
  while (texteRestant.length() > 0) {
    if (texteRestant.length() <= MAX_PAYLOAD_SIZE) {
      morceaux.push_back(texteRestant);
      break;
    }
    int splitPos = MAX_PAYLOAD_SIZE;
    for (int i = MAX_PAYLOAD_SIZE; i > MAX_PAYLOAD_SIZE - 40; i--) {
      char c = texteRestant[i];
      if (c == ' ' || c == '.' || c == ',' || c == '!') {
        splitPos = i + 1;
        break;
      }
    }
    morceaux.push_back(texteRestant.substring(0, splitPos));
    texteRestant = texteRestant.substring(splitPos);
  }

  int total = morceaux.size();
  if (total > MAX_CHUNKS)
    total = MAX_CHUNKS;
  uint32_t msgId = millis();

  for (int i = 0; i < total; i++) {
    String paquet = "C|" + requesterMac + "|" + monID + "|" + String(msgId) +
                    "|" + String(total) + "|" + String(i) + "|" + morceaux[i];
    envoyerUnicast(requesterMac, (uint8_t *)paquet.c_str(), paquet.length());
    delay(45);
    server.handleClient();
  }
}

// Transmet le document centres d'intérêt (Doc MD) d'un module par morceaux radio (ESP-NOW)
void envoyerDoc(String targetMac, String requesterMac) {
  String docText = "";
  if (LittleFS.exists("/notes.md")) {
    File file = LittleFS.open("/notes.md", "r");
    docText = file.readString();
    file.close();
  }
  if (docText.length() == 0) {
    docText = "*(Aucun centre d'intérêt renseigné par cet utilisateur)*";
  }

  std::vector<String> morceaux;
  String texteRestant = docText;
  while (texteRestant.length() > 0) {
    if (texteRestant.length() <= MAX_PAYLOAD_SIZE) {
      morceaux.push_back(texteRestant);
      break;
    }
    int splitPos = MAX_PAYLOAD_SIZE;
    for (int i = MAX_PAYLOAD_SIZE; i > MAX_PAYLOAD_SIZE - 40; i--) {
      char c = texteRestant[i];
      if (c == ' ' || c == '.' || c == ',' || c == '!') {
        splitPos = i + 1;
        break;
      }
    }
    morceaux.push_back(texteRestant.substring(0, splitPos));
    texteRestant = texteRestant.substring(splitPos);
  }

  int total = morceaux.size();
  if (total > MAX_CHUNKS)
    total = MAX_CHUNKS;
  uint32_t msgId = millis();

  for (int i = 0; i < total; i++) {
    String paquet = "D|" + requesterMac + "|" + monID + "|" + String(msgId) +
                    "|" + String(total) + "|" + String(i) + "|" + morceaux[i];
    envoyerUnicast(requesterMac, (uint8_t *)paquet.c_str(), paquet.length());
    delay(45);
    server.handleClient();
  }
}


// Découpe et transmet notre message diffusé actuel (maPensee) sur le réseau local en morceaux
void envoyerTexteLong() {
  String texteRestant = maPensee;
  std::vector<String> morceaux;

  if (texteRestant.length() == 0) {
    morceaux.push_back("");
  } else {
    while (texteRestant.length() > 0) {
      if (texteRestant.length() <= MAX_PAYLOAD_SIZE) {
        morceaux.push_back(texteRestant);
        break;
      }
      int splitPos = MAX_PAYLOAD_SIZE;
      for (int i = MAX_PAYLOAD_SIZE; i > MAX_PAYLOAD_SIZE - 40; i--) {
        char c = texteRestant[i];
        if (c == ' ' || c == '.' || c == ',' || c == '!') {
          splitPos = i + 1;
          break;
        }
      }
      morceaux.push_back(texteRestant.substring(0, splitPos));
      texteRestant = texteRestant.substring(splitPos);
    }
  }

  int total = morceaux.size();
  if (total > MAX_CHUNKS)
    total = MAX_CHUNKS;
  monMsgID = millis();

  String prefix = (monContraste == 1) ? "P1|" : "P0|";

  for (int i = 0; i < total; i++) {
    String paquet = prefix + monID + "|" + monNomReseau +
                    "::" + monNomUtilisateur + "|" + String(monMsgID) + "|" +
                    String(total) + "|" + String(i) + "|" + morceaux[i];
    esp_now_send(broadcastAddress, (uint8_t *)paquet.c_str(), paquet.length());
    delay(75);
  }
}
// Paragraphe 4.2 : envoyerUnicast et utilitaires d'adressage MAC
// Met à jour la date/heure de dernier contact d'un module voisin détecté
void rafraichirDernierContact(String mac) {
  mac.toLowerCase();
  mac.trim();
  for (auto &m : messages) {
    if (m.auteur == mac) {
      m.dernierContact = millis();
      break;
    }
  }
}

// Convertit une chaîne de caractères MAC (ex: "aa:bb:cc...") en tableau d'octets
bool stringToMac(String macStr, uint8_t *macBytes) {
  macStr.replace(":", "");
  macStr.trim();
  if (macStr.length() != 12) return false;
  for (int i = 0; i < 6; i++) {
    String byteStr = macStr.substring(i * 2, i * 2 + 2);
    macBytes[i] = (uint8_t) strtol(byteStr.c_str(), NULL, 16);
  }
  return true;
}

// Transmet un paquet radio en Unicast (direct) vers une adresse MAC cible
void envoyerUnicast(String targetMacStr, const uint8_t *payload, size_t len) {
  uint8_t targetMac[6];
  if (stringToMac(targetMacStr, targetMac)) {
    if (!esp_now_is_peer_exist(targetMac)) {
      esp_now_peer_info_t peer;
      memset(&peer, 0, sizeof(peer));
      memcpy(peer.peer_addr, targetMac, 6);
      peer.channel = 1;
      peer.encrypt = false;
      esp_now_add_peer(&peer);
    }
    esp_now_send(targetMac, payload, len);
  } else {
    esp_now_send(broadcastAddress, payload, len);
  }
}


struct PaquetRecu {
  String msg;
  int rssi;
  String senderMac;
};
std::vector<PaquetRecu> paquetsRecusQueue;
SemaphoreHandle_t queueMutex = NULL;




// Paragraphe 4.3 : Callback de réception onReceive
// Fonction de rappel invoquée par l'OS lors de la réception d'un paquet radio ESP-NOW
void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (fonctionnementMode == 1) {
    return;
  }
  String senderMac = "";
  if (info && info->src_addr) {
    char mac[13];
    snprintf(mac, sizeof(mac), "%02x%02x%02x%02x%02x%02x",
             info->src_addr[0], info->src_addr[1], info->src_addr[2],
             info->src_addr[3], info->src_addr[4], info->src_addr[5]);
    senderMac = String(mac);
    senderMac.toLowerCase();
  }
  int rssi = -100;
  if (info && info->rx_ctrl) {
    rssi = info->rx_ctrl->rssi;
  }
  char buffer[len + 1];
  memcpy(buffer, data, len);
  buffer[len] = '\0';
  String msg = String(buffer);

  if (queueMutex != NULL && xSemaphoreTake(queueMutex, portMAX_DELAY) == pdTRUE) {
    paquetsRecusQueue.push_back({msg, rssi, senderMac});
    xSemaphoreGive(queueMutex);
  }
}

// Paragraphe 4.4 : traiterPaquetRecu (Analyseur et dispatcheur de paquets radio)
// Analyse le protocole texte des paquets radio reçus (C, D, V, etc.) et exécute les actions associées
void traiterPaquetRecu(String msg, int rssi, String senderMac) {
  if (estBanni(senderMac)) {
    return;
  }

  
  if (msg.startsWith("RQ|")) {
    String targetMac = msg.substring(3);
    targetMac.toLowerCase(); targetMac.trim();
    depilerMessagesEnAttentePour(targetMac);
    return;
  }

  if (msg.startsWith("R|")) {
    int s1 = msg.indexOf('|', 2);
    if (s1 > 0) {
      String targetMac = msg.substring(2, s1);
      String requesterMac = msg.substring(s1 + 1);
      targetMac.toLowerCase(); targetMac.trim();
      requesterMac.toLowerCase(); requesterMac.trim();
      if (targetMac == monID) {
        cvRequestsQueue.push_back({targetMac, requesterMac});
      }
    }
    return;
  }

  if (msg.startsWith("Q|")) {
    int s1 = msg.indexOf('|', 2);
    if (s1 > 0) {
      String targetMac = msg.substring(2, s1);
      String requesterMac = msg.substring(s1 + 1);
      targetMac.toLowerCase(); targetMac.trim();
      requesterMac.toLowerCase(); requesterMac.trim();
      if (targetMac == monID) {
        docRequestsQueue.push_back({targetMac, requesterMac});
      }
    }
    return;
  }

  
  if (msg.startsWith("C|")) {
    int s1 = msg.indexOf('|', 2);
    int s2 = msg.indexOf('|', s1 + 1);
    int s3 = msg.indexOf('|', s2 + 1);
    int s4 = msg.indexOf('|', s3 + 1);
    int s5 = msg.indexOf('|', s4 + 1);
    if (s5 > 0) {
      String requesterMac = msg.substring(2, s1);
      String sourceMac = msg.substring(s1 + 1, s2);
      String msgId = msg.substring(s2 + 1, s3);
      int total = msg.substring(s3 + 1, s4).toInt();
      int index = msg.substring(s4 + 1, s5).toInt();
      String chunk = msg.substring(s5 + 1);

      requesterMac.toLowerCase(); requesterMac.trim();
      sourceMac.toLowerCase(); sourceMac.trim();
      if (requesterMac == monID) {
        rafraichirDernierContact(sourceMac);
        String cleUnique = "CV_" + sourceMac + "_" + msgId;
        if (cvsEnAttente.find(cleUnique) == cvsEnAttente.end()) {
          MessageIncomplet mi;
          mi.nomReseau = "";
          mi.total = total;
          mi.recu = 0;
          mi.rssi = 0;
          mi.morceaux.resize(total);
          mi.dernierUpdate = millis();
          mi.contrast = 0;
          cvsEnAttente[cleUnique] = mi;
        }
        if (cvsEnAttente[cleUnique].morceaux[index] == "") {
          cvsEnAttente[cleUnique].morceaux[index] = chunk;
          cvsEnAttente[cleUnique].recu++;
          cvsEnAttente[cleUnique].dernierUpdate = millis();
        }
        if (cvsEnAttente[cleUnique].recu == total) {
          String cvComplet = "";
          for (int i = 0; i < total; i++) {
            cvComplet += cvsEnAttente[cleUnique].morceaux[i];
          }
          cvsStockes[sourceMac] = cvComplet;
          cvsEnAttente.erase(cleUnique);
        }
      }
      return;
    }
  }

  if (msg.startsWith("D|")) {
    int s1 = msg.indexOf('|', 2);
    int s2 = msg.indexOf('|', s1 + 1);
    int s3 = msg.indexOf('|', s2 + 1);
    int s4 = msg.indexOf('|', s3 + 1);
    int s5 = msg.indexOf('|', s4 + 1);
    if (s5 > 0) {
      String requesterMac = msg.substring(2, s1);
      String sourceMac = msg.substring(s1 + 1, s2);
      String msgId = msg.substring(s2 + 1, s3);
      int total = msg.substring(s3 + 1, s4).toInt();
      int index = msg.substring(s4 + 1, s5).toInt();
      String chunk = msg.substring(s5 + 1);

      requesterMac.toLowerCase(); requesterMac.trim();
      sourceMac.toLowerCase(); sourceMac.trim();
      if (requesterMac == monID) {
        rafraichirDernierContact(sourceMac);
        String cleUnique = "DOC_" + sourceMac + "_" + msgId;
        if (docsEnAttente.find(cleUnique) == docsEnAttente.end()) {
          MessageIncomplet mi;
          mi.nomReseau = "";
          mi.total = total;
          mi.recu = 0;
          mi.rssi = 0;
          mi.morceaux.resize(total);
          mi.dernierUpdate = millis();
          mi.contrast = 0;
          docsEnAttente[cleUnique] = mi;
        }
        if (docsEnAttente[cleUnique].morceaux[index] == "") {
          docsEnAttente[cleUnique].morceaux[index] = chunk;
          docsEnAttente[cleUnique].recu++;
          docsEnAttente[cleUnique].dernierUpdate = millis();
        }
        if (docsEnAttente[cleUnique].recu == total) {
          String docComplet = "";
          for (int i = 0; i < total; i++) {
            docComplet += docsEnAttente[cleUnique].morceaux[i];
          }
          docsStockes[sourceMac] = docComplet;
          docsEnAttente.erase(cleUnique);
        }
      }
      return;
    }
  }
  




  

  

  

  

  if (msg.startsWith("V|")) {

    int s1 = msg.indexOf('|', 2);
    int s2 = msg.indexOf('|', s1 + 1);
    if (s1 > 0 && s2 > 0) {
      String cible = msg.substring(2, s1);
      String voteur = msg.substring(s1 + 1, s2);
      String emoticon = msg.substring(s2 + 1);
      votesMap[cible][emoticon]++;
      if (cible == monID) {
        voteNotifications.push_back({voteur, emoticon, millis()});
        if (voteNotifications.size() > 5) {
          voteNotifications.erase(voteNotifications.begin());
        }
      }
    }
    return;
  }

  
  if (msg.startsWith("GF|")) {
    int s1 = msg.indexOf('|', 3);
    int s2 = msg.indexOf('|', s1 + 1);
    if (s1 > 0 && s2 > 0) {
      String cibleID = msg.substring(3, s1);
      String auteur = msg.substring(s1 + 1, s2);
      String texte = msg.substring(s2 + 1);

      auteur.replace("|||", " ");
      texte.replace("|||", " ");
      texte.replace("\n", " ");

      if (cibleID == monID) {
        String displayAuteur = auteur;
        int sepIdx = auteur.indexOf("::");
        if (sepIdx > 0) {
          displayAuteur = auteur.substring(sepIdx + 2); 
        }

        String macStr = senderMac;

        String storedAuthor = displayAuteur;
        if (macStr.length() > 0) {
          storedAuthor = macStr + "::" + displayAuteur;
        }

        String color = "#10b981"; 
        String newLine =
            storedAuthor + "|||" + texte + "|||0|||0|||" + color + "|||0|||1.5";

        String lines[MAX_GRAFFITIS];
        int count = 0;
        File file = LittleFS.open(GRAFFITIS_FILE, "r");
        if (file) {
          while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.length() > 0) {
              if (count < MAX_GRAFFITIS)
                lines[count++] = line;
              else {
                for (int i = 0; i < MAX_GRAFFITIS - 1; i++)
                  lines[i] = lines[i + 1];
                lines[MAX_GRAFFITIS - 1] = line;
              }
            }
          }
          file.close();
        }

        if (count < MAX_GRAFFITIS)
          lines[count++] = newLine;
        else {
          for (int i = 0; i < MAX_GRAFFITIS - 1; i++)
            lines[i] = lines[i + 1];
          lines[MAX_GRAFFITIS - 1] = newLine;
        }

        File outFile = LittleFS.open(GRAFFITIS_FILE, "w");
        for (int i = 0; i < count; i++)
          outFile.println(lines[i]);
        outFile.close();
      }
    }
    return;
  }

  
  int contrast = 0;
  if (msg.startsWith("P0|")) {
    contrast = 0;
    msg = msg.substring(3);
  } else if (msg.startsWith("P1|")) {
    contrast = 1;
    msg = msg.substring(3);
  } else if (msg.startsWith("P|")) {
    contrast = 0;
    msg = msg.substring(2);
  } else {
    return; 
  }

  int s[5];
  s[0] = msg.indexOf('|');
  for (int i = 1; i < 5; i++)
    s[i] = msg.indexOf('|', s[i - 1] + 1);

  if (s[4] > 0) {
    String auteur = msg.substring(0, s[0]);
    String nom = msg.substring(s[0] + 1, s[1]);
    String msgId = msg.substring(s[1] + 1, s[2]);
    int total = msg.substring(s[2] + 1, s[3]).toInt();
    int index = msg.substring(s[3] + 1, s[4]).toInt();
    String texteChunk = msg.substring(s[4] + 1);
    String cleUnique = auteur + "_" + msgId;

    if (index >= MAX_CHUNKS || index < 0)
      return;

    if (messagesEnAttente.find(cleUnique) == messagesEnAttente.end()) {
      MessageIncomplet mi;
      mi.nomReseau = nom;
      mi.total = total;
      mi.recu = 0;
      mi.rssi = rssi;
      mi.morceaux.resize(total);
      mi.dernierUpdate = millis();
      mi.contrast = contrast;
      messagesEnAttente[cleUnique] = mi;
    }

    if (messagesEnAttente[cleUnique].morceaux[index] == "") {
      messagesEnAttente[cleUnique].morceaux[index] = texteChunk;
      messagesEnAttente[cleUnique].recu++;
      messagesEnAttente[cleUnique].rssi = rssi;
    }

    if (messagesEnAttente[cleUnique].recu == total) {
      String texteComplet = "";
      for (int i = 0; i < total; i++)
        texteComplet += messagesEnAttente[cleUnique].morceaux[i];

      bool found = false;
      for (auto &m : messages) {
        if (m.auteur == auteur) {
          if (m.texte != texteComplet) {
            votesMap[auteur].clear(); // Reset count for new message
          }
          m.nomReseau = nom;
          m.texte = texteComplet;
          m.rssi = rssi;
          m.dernierContact = millis();
          m.contrast = messagesEnAttente[cleUnique].contrast;
          found = true;
          break;
        }
      }
      if (!found) {
        votesMap[auteur].clear(); // Reset count for first message
        messages.push_back({auteur, nom, texteComplet, rssi, millis(), messagesEnAttente[cleUnique].contrast});
        if (messages.size() > MAX_MESSAGES)
          messages.erase(messages.begin());
      }

      
      String ssidPart = nom;
      String userPart = nom;
      int sepIdx = nom.indexOf("::");
      if (sepIdx > 0) {
        ssidPart = nom.substring(0, sepIdx);
        userPart = nom.substring(sepIdx + 2);
      }

      
      ajouterAuJournal(userPart, ssidPart, texteComplet);

      messagesEnAttente.erase(cleUnique);
      resauvegarder = true;
    }
  }
}


// CHAPITRE 5 : SERVEUR WEB ET INTERFACES D'API
// Paragraphe 5.1 : handleStaticFile (Serveur de fichiers statiques)
// Recherche et envoie les fichiers statiques (index.html, etc.) demandés au serveur web
bool handleStaticFile(String path, bool forceDownload = false) {
  if (LittleFS.exists(path)) {
    String contentType = "text/plain";
    if (path.endsWith(".html")) {
      contentType = "text/html";
      sendNoCacheHeaders();
    }
    else if (path.endsWith(".css"))
      contentType = "text/css";
    else if (path.endsWith(".js"))
      contentType = "application/javascript";
    else if (path.endsWith(".pdf"))
      contentType = "application/pdf";
    else if (path.endsWith(".png"))
      contentType = "image/png";
    else if (path.endsWith(".jpg"))
      contentType = "image/jpeg";
    else if (path.endsWith(".zip"))
      contentType = "application/zip";
    else if (path.endsWith(".ino"))
      contentType = "text/plain";

    if (forceDownload) {
      int lastSlash = path.lastIndexOf('/');
      String filename = (lastSlash >= 0) ? path.substring(lastSlash + 1) : path;
      server.sendHeader("Content-Disposition",
                        "attachment; filename=\"" + filename + "\"");
    }

    File file = LittleFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}


// Paragraphe 5.2 : API Graffitis (lire, écrire et effacer les graffitis/messages privés)
// API récupérant la liste des graffitis locaux stockés
void handleGetGraffitis() {
  File file = LittleFS.open(GRAFFITIS_FILE, "r");
  if (!file) {
    server.send(500, "application/json", "[]");
    return;
  }

  String json = "[";
  bool first = true;
  int index = 0;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      String pieces[7];
      int count = 0;
      int startIdx = 0;
      while (startIdx < line.length() && count < 7) {
        int nextIdx = line.indexOf("|||", startIdx);
        if (nextIdx == -1) {
          pieces[count++] = line.substring(startIdx);
          break;
        }
        pieces[count++] = line.substring(startIdx, nextIdx);
        startIdx = nextIdx + 3;
      }

      if (count >= 2) {
        if (!first)
          json += ",";
        String author = pieces[0];
        String msg = pieces[1];
        author.replace("\"", "\\\"");
        msg.replace("\"", "\\\"");

        json += "{\"index\":" + String(index) + ", \"author\":\"" + author +
                "\", \"text\":\"" + msg + "\"";
        if (count >= 7) {
          json += ", \"x\":\"" + pieces[2] + "\", \"y\":\"" + pieces[3] +
                  "\", \"color\":\"" + pieces[4] + "\", \"rot\":\"" +
                  pieces[5] + "\", \"size\":\"" + pieces[6] + "\"";
        }
        json += "}";
        first = false;
        index++;
      }
    }
  }
  file.close();
  json += "]";

  server.send(200, "application/json", json);
}

// API publiant un nouveau graffiti local rédigé par l'utilisateur
void handlePostGraffiti() {
  if (!server.hasArg("message")) {
    server.send(400, "text/plain", "Requête invalide");
    return;
  }

  
  String author = monNomUtilisateur;
  String message = server.arg("message");
  String x = server.hasArg("x") ? server.arg("x") : "0";
  String y = server.hasArg("y") ? server.arg("y") : "0";
  String color = server.hasArg("color") ? server.arg("color") : "";
  String rot = server.hasArg("rot") ? server.arg("rot") : "";
  String sz = server.hasArg("size") ? server.arg("size") : "";

  auto cleanStr = [](String &s) {
    s.replace("\n", " ");
    s.replace("|||", " ");
  };
  cleanStr(author);
  cleanStr(message);
  cleanStr(x);
  cleanStr(y);
  cleanStr(color);
  cleanStr(rot);
  cleanStr(sz);

  if (message.length() == 0) {
    server.send(400, "text/plain", "Texte vide interdit");
    return;
  }

  
  String timeStr = obtenirTempsFormate();
  if (timeStr.length() > 0) {
    message += " (" + timeStr.substring(1, timeStr.length() - 1) + ")";
  }

  String lines[MAX_GRAFFITIS];
  int count = 0;

  File file = LittleFS.open(GRAFFITIS_FILE, "r");
  if (file) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) {
        if (count < MAX_GRAFFITIS) {
          lines[count++] = line;
        } else {
          for (int i = 0; i < MAX_GRAFFITIS - 1; i++)
            lines[i] = lines[i + 1];
          lines[MAX_GRAFFITIS - 1] = line;
        }
      }
    }
    file.close();
  }

  String newLine = author + "|||" + message + "|||" + x + "|||" + y + "|||" +
                   color + "|||" + rot + "|||" + sz;

  if (count < MAX_GRAFFITIS) {
    lines[count++] = newLine;
  } else {
    for (int i = 0; i < MAX_GRAFFITIS - 1; i++)
      lines[i] = lines[i + 1];
    lines[MAX_GRAFFITIS - 1] = newLine;
  }

  File outFile = LittleFS.open(GRAFFITIS_FILE, "w");
  for (int i = 0; i < count; i++)
    outFile.println(lines[i]);
  outFile.close();

  server.send(200, "text/plain", "Message enregistré");
}

// API recevant et stockant un graffiti envoyé à distance par radio par un voisin
// Fonctions pour la mise en attente (DTN) des messages privés hors ligne
void ajouterAMessagesEnAttente(String cible, String message) {
  cible.toLowerCase(); cible.trim();
  message.replace("\n", " ");
  message.replace("|||", " ");
  File f = LittleFS.open("/pending_direct.txt", "a");
  if (f) {
    f.println(cible + "|||" + message);
    f.close();
  }
}

void depilerMessagesEnAttentePour(String targetMac) {
  targetMac.toLowerCase(); targetMac.trim();
  if (!LittleFS.exists("/pending_direct.txt")) return;
  std::vector<String> conservees;
  File f = LittleFS.open("/pending_direct.txt", "r");
  if (f) {
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) continue;
      int idx = line.indexOf("|||");
      if (idx > 0) {
        String cible = line.substring(0, idx);
        String msg = line.substring(idx + 3);
        cible.toLowerCase(); cible.trim();
        if (cible == targetMac) {
          String timeStr = obtenirTempsFormate();
          if (timeStr.length() > 0) {
            msg += " (" + timeStr.substring(1, timeStr.length() - 1) + ")";
          }
          String paquet = "GF|" + targetMac + "|" + monNomReseau + "::" + monNomUtilisateur + "|" + msg;
          esp_now_send(broadcastAddress, (uint8_t *)paquet.c_str(), paquet.length());
          delay(40);
        } else {
          conservees.push_back(line);
        }
      }
    }
    f.close();
  }
  File fOut = LittleFS.open("/pending_direct.txt", "w");
  if (fOut) {
    for (String const &line : conservees) {
      fOut.println(line);
    }
    fOut.close();
  }
}

void handleRemoteGraffiti() {
  if (!server.hasArg("cible") || !server.hasArg("message")) {
    server.send(400, "text/plain", "Requête invalide");
    return;
  }
  String cible = server.arg("cible");
  String message = server.arg("message");
  message.replace("\n", " ");
  message.replace("|||", " ");
  if (message.length() > 150)
    message = message.substring(0, 150);

  bool online = false;
  cible.toLowerCase(); cible.trim();
  for (int i = 0; i < messages.size(); i++) {
    if (messages[i].auteur == cible && (millis() - messages[i].dernierContact < 25000)) {
      online = true;
      break;
    }
  }

  if (online) {
    String timeStr = obtenirTempsFormate();
    if (timeStr.length() > 0) {
      message += " (" + timeStr.substring(1, timeStr.length() - 1) + ")";
    }
    String paquet = "GF|" + cible + "|" + monNomReseau + "::" + monNomUtilisateur + "|" + message;
    esp_now_send(broadcastAddress, (uint8_t *)paquet.c_str(), paquet.length());
    server.send(200, "text/plain", "ENVOYE:Le message a été transmis directement.");
  } else {
    ajouterAMessagesEnAttente(cible, message);
    server.send(200, "text/plain", "PENDING:Le destinataire est hors de portée. Votre message a été mis en attente et sera envoyé automatiquement dès qu'il sera à portée.");
  }
}




// API supprimant un graffiti spécifique de la liste
void handleDeleteGraffiti() {
  if (!server.hasArg("index")) {
    server.send(400, "text/plain", "Index manquant");
    return;
  }
  int targetIndex = server.arg("index").toInt();

  
  File file = LittleFS.open(GRAFFITIS_FILE, "r");
  if (!file) {
    server.send(500, "text/plain", "Impossible d'ouvrir le fichier");
    return;
  }

  std::vector<String> lines;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      lines.push_back(line);
    }
  }
  file.close();

  
  if (targetIndex < 0 || targetIndex >= (int)lines.size()) {
    server.send(400, "text/plain", "Index invalide");
    return;
  }

  
  File outFile = LittleFS.open(GRAFFITIS_FILE, "w");
  if (!outFile) {
    server.send(500, "text/plain", "Erreur ecriture");
    return;
  }

  for (int i = 0; i < (int)lines.size(); i++) {
    if (i != targetIndex) {
      outFile.println(lines[i]);
    }
  }
  outFile.close();

  server.send(200, "text/plain", "OK");
}


// Paragraphe 5.3 : API Configuration (Configuration réseau et mode de fonctionnement)
// API renvoyant la configuration actuelle du module au format JSON
void handleGetConfig() {
  String favJson = "[";
  for (size_t i = 0; i < favorites.size(); i++) {
    if (i > 0) favJson += ",";
    favJson += "\"" + favorites[i] + "\"";
  }
  favJson += "]";

  String banJson = "[";
  for (size_t i = 0; i < banned.size(); i++) {
    if (i > 0) banJson += ",";
    banJson += "\"" + banned[i] + "\"";
  }
  banJson += "]";

  bool isSalonHost = true;
  if (fonctionnementMode == 1) {
    String clientIP = server.client().remoteIP().toString();
    if (!salonHostConnected) {
      salonHostConnected = true;
      salonHostIP = clientIP;
    }
    isSalonHost = (clientIP == salonHostIP);
  }

  String json = "{\"ssid\":\"" + monNomReseau + "\",\"pwd\":\"" +
                monMotDePasse + "\",\"rebootMsg\":\"" + monMessageReboot +
                "\",\"username\":\"" + monNomUtilisateur + "\",\"mode\":" +
                String(fonctionnementMode) + ",\"fsTotal\":" + String(LittleFS.totalBytes()) + 
                ",\"fsUsed\":" + String(LittleFS.usedBytes()) + ",\"favorites\":" + favJson +
                ",\"banned\":" + banJson + ",\"contrast\":" + String(monContraste) +
                ",\"rediffuseLast\":" + String(rediffuserDernier) + 
                ",\"isSalonHost\":" + (isSalonHost ? "true" : "false") +
                "}";
  server.send(200, "application/json", json);
}

// API recevant et appliquant une nouvelle configuration de paramètres réseau et redémarrant
void handlePostConfig() {
  if (server.hasArg("ssid") && server.hasArg("pwd")) {
    String newSsid = server.arg("ssid");
    String newPwd = server.arg("pwd");
    String newRebootMsg =
        server.hasArg("rebootMsg") ? server.arg("rebootMsg") : "";
    String newUsername =
        server.hasArg("username") ? server.arg("username") : "";
    String newMode =
        server.hasArg("mode") ? server.arg("mode") : String(fonctionnementMode);
    String newRediffuse =
        server.hasArg("rediffuseLast") ? server.arg("rediffuseLast") : "0";

    newSsid.trim();
    newPwd.trim();
    newRebootMsg.trim();
    newUsername.trim();
    newMode.trim();

    newSsid.replace("\r", "");
    newSsid.replace("\n", "");
    newPwd.replace("\r", "");
    newPwd.replace("\n", "");
    newRebootMsg.replace("\r", " ");
    newRebootMsg.replace("\n", " ");
    newUsername.replace("\r", "");
    newUsername.replace("\n", "");
    newMode.replace("\r", "");
    newMode.replace("\n", "");
    newRediffuse.replace("\r", "");
    newRediffuse.replace("\n", "");

    int nMode = newMode.toInt();
    if (nMode == 1) {
      static unsigned long derniereRequeteAttente = 0;
  if (millis() - derniereRequeteAttente > 15000) {
    derniereRequeteAttente = millis();
    String paquet = "RQ|" + monID;
    esp_now_send(broadcastAddress, (uint8_t *)paquet.c_str(), paquet.length());
  }

  if (fonctionnementMode == 0) {
        monNomUtilisateurP2P = monNomUtilisateur;
      }
      newUsername = "L'hébergeur";
    } else {
      if (newUsername == "L'hébergeur" || newUsername.length() == 0) {
        if (monNomUtilisateurP2P.length() > 0) {
          newUsername = monNomUtilisateurP2P;
        } else {
          newUsername = monNomReseau;
        }
      }
      monNomUtilisateurP2P = newUsername;
    }

    if (newSsid.length() > 0) {
      File f = LittleFS.open(CONFIG_FILE, "w");
      if (f) {
        f.println(newSsid);
        f.println(newPwd);
        f.println(newRebootMsg);
        f.println(newUsername);
        f.println(newMode);
        f.println(newRediffuse);
        f.println(monNomUtilisateurP2P);
        f.close();
        server.send(200, "text/plain", "OK");
        delay(500);
        ESP.restart();
      } else {
        server.send(500, "text/plain", "Erreur écriture");
      }
    } else {
      server.send(400, "text/plain", "SSID invalide");
    }
  } else {
    server.send(400, "text/plain", "Paramètres manquants");
  }
}


// CHAPITRE 6 : PAGE WEB PRINCIPALE (CODE CLIENT EMBARQUÉ - HTML/CSS/JS)
// Paragraphe 6.1 : htmlPage - Structure HTML de l'application
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover">
  <title>Civvi - Tableau de Bord Cyber-Naturel</title>
  <!-- Paragraphe 6.2 : Styles CSS (Thèmes, contrastes, responsive) -->
  <style>
    /* Premium Theme Variables */
    :root {
      --bg: #141310;
      --surface: #21201b;
      --surface-high: #2b2a26;
      --surface-low: #1d1c18;
      --primary: #d9952b;
      --secondary: #bacf88;
      --text: #e6e2db;
      --text-muted: #a6a49f;
      --border: rgba(217, 149, 43, 0.15);
      --border-focus: #d9952b;
      --shadow: 0 8px 30px rgba(0, 0, 0, 0.5);
    }

    /* Salon Mode Theme Override (Tech Blue/Indigo) */
    body.salon-mode {
      --bg: #0b0f19;
      --surface: #121826;
      --surface-high: #1c2538;
      --surface-low: #0a0d16;
      --primary: #5c7cfa;
      --secondary: #74c0fc;
      --border: rgba(92, 124, 250, 0.2);
      --border-focus: #5c7cfa;
    }
    body.salon-mode .c1 { background: #2563eb; }
    body.salon-mode .c3 { background: #ca8a04; }
    body.salon-mode .c11 { background: #dc2626; }
    body.salon-mode .c12 { background: #7c3aed; }
    
    /* Visual Avatar Preview Styles */
    .avatar-preview-box {
      display: flex;
      flex-direction: column;
      align-items: center;
      width: 120px;
      padding: 10px;
      background: rgba(0, 0, 0, 0.2);
      border: 1px solid var(--border);
      border-radius: 8px;
    }
    .avatar-head {
      width: 50px;
      height: 40px;
      border-radius: 25px 25px 0 0;
      border: 2px solid var(--border);
      background-color: #e5e7eb;
      position: relative;
      transition: all 0.3s ease;
    }
    .avatar-torso {
      width: 70px;
      height: 90px;
      border: 2px solid var(--border);
      background-color: #e5e7eb;
      position: relative;
      margin-top: 4px;
      transition: all 0.3s ease;
      overflow: hidden;
    }
    .avatar-legs {
      width: 60px;
      height: 100px;
      border: 2px solid var(--border);
      background-color: #e5e7eb;
      position: relative;
      margin-top: 4px;
      transition: all 0.3s ease;
      overflow: hidden;
      display: flex;
    }
    .avatar-leg-left, .avatar-leg-right {
      width: 50%;
      height: 100%;
    }
    .avatar-leg-left {
      border-right: 1px solid rgba(0,0,0,0.15);
    }
    .avatar-feet {
      width: 66px;
      height: 20px;
      display: flex;
      justify-content: space-between;
      margin-top: 2px;
    }
    .avatar-foot {
      width: 26px;
      height: 100%;
      border: 2px solid var(--border);
      background-color: #e5e7eb;
      transition: all 0.3s ease;
    }
    
    /* Patterns */
    .pattern-overlay {
      position: absolute;
      inset: 0;
      pointer-events: none;
      opacity: 0.3;
      background-size: 15px 15px;
    }
    .pattern-stripes {
      background-image: linear-gradient(45deg, #000 25%, transparent 25%, transparent 50%, #000 50%, #000 75%, transparent 75%, transparent);
    }
    .pattern-dots {
      background-image: radial-gradient(#000 20%, transparent 20%);
    }
    .pattern-geometric {
      background-image: linear-gradient(45deg, #000 25%, transparent 25%), linear-gradient(-45deg, #000 25%, transparent 25%), linear-gradient(45deg, transparent 75%, #000 75%), linear-gradient(-45deg, transparent 75%, #000 75%);
      background-size: 8px 8px;
    }
    
    /* Control layouts */
    .avatar-editor-layout {
      display: flex;
      gap: 20px;
      width: 100%;
      align-items: flex-start;
      margin-top: 15px;
    }
    @media (max-width: 480px) {
      .avatar-editor-layout {
        flex-direction: column;
        align-items: center;
      }
    }
    .avatar-controls-column {
      display: flex;
      flex-direction: column;
      gap: 12px;
      flex-grow: 1;
      width: 100%;
    }
    .color-swatches-row {
      display: flex;
      flex-wrap: wrap;
      gap: 6px;
    }
    .color-swatch-btn {
      width: 24px;
      height: 24px;
      border-radius: 4px;
      cursor: pointer;
      border: 1px solid rgba(255,255,255,0.2);
      transition: transform 0.2s;
    }
    .color-swatch-btn:hover, .color-swatch-btn.active {
      transform: scale(1.15);
      border-color: var(--primary);
      box-shadow: 0 0 5px var(--primary);
    }
    .pattern-btns-row {
      display: flex;
      gap: 6px;
      align-items: center;
    }
    .pattern-select-btn {
      width: 24px;
      height: 24px;
      border: 1px solid rgba(255,255,255,0.2);
      border-radius: 4px;
      display: flex;
      align-items: center;
      justify-content: center;
      background: rgba(0, 0, 0, 0.3);
      cursor: pointer;
    }
    .pattern-select-btn:hover, .pattern-select-btn.active {
      border-color: var(--primary);
      background: rgba(217, 149, 43, 0.15);
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    
    body, html {
      margin: 0; padding: 0;
      width: 100%; min-height: 100vh;
      min-height: 100dvh;
      overflow-x: hidden;
      overflow-y: scroll;
      background-color: var(--bg);
      color: var(--text);
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      background-image: url("data:image/svg+xml,%3Csvg width='80' height='80' xmlns='http://www.w3.org/2000/svg'%3E%3Cfilter id='noise'%3E%3CfeTurbulence type='fractalNoise' baseFrequency='0.8' numOctaves='3' stitchTiles='stitch'/%3E%3C/filter%3E%3Crect width='100%25' height='100%25' filter='url(%23noise)' opacity='0.04'/%3E%3C/svg%3E");
    }

    /* Conteneur Principal Flexible */
    .esp-interface {
      display: flex;
      flex-direction: column;
      width: 100%;
      min-height: 100vh;
      min-height: 100dvh;
      position: relative;
    }

    /* Background Calque Lignes */
    .lignes-fond {
      position: absolute;
      top: 0;
      left: 0;
      width: 100%;
      height: 100%;
      background: repeating-linear-gradient(
        transparent,
        transparent 59px,
        rgba(217, 149, 43, 0.04) 60px
      );
      z-index: 1;
      pointer-events: none;
    }

    /* Top Bar */
    .top-bar {
      background: rgba(33, 32, 27, 0.7);
      backdrop-filter: blur(10px);
      border-bottom: 1px solid var(--border);
      display: flex;
      flex-direction: column;
      justify-content: center;
      padding: 0 90px;
      z-index: 10;
      gap: 4px;
      height: 80px;
      flex-shrink: 0;
    }
    .top-info {
      font-size: 11px;
      text-transform: uppercase;
      letter-spacing: 1.5px;
      color: var(--text-muted);
      display: flex;
      align-items: center;
      gap: 15px;
    }
    .badge {
      background: rgba(186, 207, 136, 0.12);
      color: var(--secondary);
      border: 1px solid rgba(186, 207, 136, 0.2);
      padding: 3px 8px;
      border-radius: 20px;
      font-weight: bold;
    }
    .header-title {
      font-family: Georgia, serif;
      font-style: italic;
      color: var(--primary);
      font-size: 20px;
      letter-spacing: 2px;
    }

    /* Bottom Bar */
    .bottom-bar {
      background: rgba(33, 32, 27, 0.5);
      border-top: 1px solid var(--border);
      display: flex;
      justify-content: center;
      align-items: center;
      cursor: pointer;
      overflow: hidden;
      position: relative;
      z-index: 10;
      transition: background 0.3s;
      padding: 0 90px;
      height: 50px;
      flex-shrink: 0;
    }
    .bottom-bar:hover {
      background: rgba(217, 149, 43, 0.08);
    }
    .marquee-actu {
      position: absolute;
      white-space: nowrap;
      color: var(--text);
      font-size: 14px;
      font-style: italic;
      will-change: transform;
    }

    @keyframes defilementG-D {
      0% { left: 0; transform: translateX(-100%); }
      100% { left: 100%; transform: translateX(0); }
    }
    .my-thought-marquee {
      display: inline-block;
      white-space: nowrap;
      color: var(--primary);
      font-size: 11px;
      font-style: italic;
      position: absolute;
      will-change: transform;
      animation: defilementG-D 20s linear infinite;
    }
    .my-thought-marquee-container {
      width: 100%;
      overflow: hidden;
      position: relative;
      height: 16px;
      border-top: 1px dashed var(--border);
      padding-top: 2px;
    }

    @keyframes scrollUpTract {
      0% { transform: translateY(180px); }
      100% { transform: translateY(0px); }
    }
    .tract-scrolling {
      animation: scrollUpTract 12s linear forwards;
    }

    /* Control Bar */
    .control-bar {
      background: #091224 !important; /* Deep blue background */
      backdrop-filter: blur(10px);
      border-top: 1px solid var(--border);
      display: flex;
      flex-direction: column;
      padding: 10px 90px;
      gap: 8px;
      z-index: 15;
      flex-shrink: 0;
    }
    .speed-box {
      display: flex;
      flex-direction: column;
      gap: 2px;
      min-width: 120px;
    }
    .speed-box span {
      font-size: 10px;
      text-transform: uppercase;
      color: var(--text-muted);
      letter-spacing: 0.5px;
    }
    .speed-box input[type="range"] {
      width: 100%;
      height: 6px;
      accent-color: var(--primary);
      cursor: pointer;
    }
    .msg-box {
      display: flex;
      flex-grow: 1;
      align-items: center;
      gap: 8px;
      background: rgba(0, 0, 0, 0.3);
      border: 1px solid var(--border);
      border-radius: 6px;
      padding: 6px 12px;
      position: relative;
    }
    .msg-box input[type="text"] {
      background: none;
      border: none;
      color: var(--text);
      font-size: 13px;
      outline: none;
      width: 100%;
      padding-right: 85px;
    }
    .msg-box #char-counter {
      font-size: 10px;
      color: var(--text-muted);
      position: absolute;
      right: 40px;
      top: 50%;
      transform: translateY(-50%);
    }

    .spinner {
      border: 4px solid rgba(255, 255, 255, 0.1);
      width: 32px;
      height: 32px;
      border-radius: 50%;
      border-left-color: var(--primary);
      animation: spin 1s linear infinite;
      margin: 20px auto;
    }
    @keyframes spin {
      0% { transform: rotate(0deg); }
      100% { transform: rotate(360deg); }
    }



    /* Corner Buttons */
    .btn-coin { 
      cursor: pointer; position: absolute; z-index: 100; 
      transition: all 0.4s cubic-bezier(0.4, 0, 0.2, 1); 
      display: flex; justify-content: center; align-items: center;
      border: 1px solid var(--border);
    }
    .btn-coin:hover {
      filter: brightness(1.3);
      box-shadow: 0 0 15px var(--primary);
    }
    
    /* Top Left (Profil) */
    .c1 { 
      top: 0; left: 0; 
      width: 80px; height: 80px; 
      background: #1d4ed8;
      border-radius: 0 0 80px 0; 
    }
    .c1 svg { width: 22px; height: 22px; margin-right: 14px; margin-bottom: 14px; }

    /* Top Right (Messagerie) */
    .c3 { 
      top: 0; right: 0; 
      width: 80px; height: 80px; 
      background: #ca8a04;
      border-radius: 0 0 0 80px; 
    }
    .c3 svg { width: 22px; height: 22px; margin-left: 14px; margin-bottom: 14px; }
    
    /* Bottom Left (Configuration) */
    .c11 { 
      bottom: 0; left: 0; 
      width: 80px; height: 80px; 
      background: #dc2626;
      border-radius: 0 80px 0 0; 
    }
    .c11 svg { width: 22px; height: 22px; margin-right: 14px; margin-top: 14px; }
    
    /* Bottom Right (Accueil - Home) */
    .c12 { 
      bottom: 0; right: 0; 
      width: 80px; height: 80px; 
      background: #7c3aed;
      border-radius: 80px 0 0 0; 
      z-index: 2150; 
    }
    .c12 svg { width: 22px; height: 22px; margin-left: 14px; margin-top: 14px; }

    /* Double corner button sizes on Mobile/Smartphone viewport */
    @media (max-width: 480px) {
      .btn-coin {
        width: 130px !important;
        height: 130px !important;
      }
      .c1 { border-radius: 0 0 130px 0 !important; }
      .c3 { border-radius: 0 0 0 130px !important; }
      .c11 { border-radius: 0 130px 0 0 !important; }
      .c12 { border-radius: 130px 0 0 0 !important; }
      .btn-coin svg {
        width: 32px !important;
        height: 32px !important;
      }
      .c1 svg { margin-right: 28px !important; margin-bottom: 28px !important; }
      .c3 svg { margin-left: 28px !important; margin-bottom: 28px !important; }
      .c11 svg { margin-right: 28px !important; margin-top: 28px !important; }
      .c12 svg { margin-left: 28px !important; margin-top: 28px !important; }
    }

    /* Style for neighbors who have signed the code of conduct (contrast === 1) */
    .esp-line.neighbor-contrast .riviere {
      border: 2.5px solid #d9952b !important;
      box-shadow: 0 0 12px rgba(217, 149, 43, 0.55) !important;
      border-radius: 6px;
    }

    /* Lorem Ipsum overlay animations from bottom-right */
    #ov-lorem { background: #321463; clip-path: circle(0% at 100% 100%); }
    #ov-lorem.actif { clip-path: circle(200vmax at 100% 100%); }

    /* Overlays */
    .overlay {
      position: fixed; top: 0; left: 0; width: 100vw; height: 100vh; height: 100dvh;
      z-index: 2000; display: flex; flex-direction: column; justify-content: flex-start; align-items: center;
      color: var(--text); pointer-events: none;
      transition: clip-path 0.8s cubic-bezier(0.65, 0, 0.35, 1), opacity 0.5s ease-in-out;
      padding: 80px 20px 70px 20px;
      box-sizing: border-box;
      overflow-y: auto;
      opacity: 0;
      visibility: hidden;
    }
    .overlay.actif { pointer-events: auto; opacity: 1; visibility: visible; }

    #ov-profil { background: #0d2468; clip-path: circle(0% at 0% 0%); } 
    #ov-messagerie { background: #513702; clip-path: circle(0% at 100% 0%); } 
    #ov-configuration { background: #590f0f; clip-path: circle(0% at 0% 100%); } 
    #ov-actu { background: rgba(15, 14, 12, 0.98); clip-path: circle(0% at 50% 100%); z-index: 2010; } 
    #ov-profil-viewer { background: rgba(20, 19, 16, 0.98); clip-path: circle(0% at 50% 50%); z-index: 2100; display: flex; flex-direction: column; justify-content: center; align-items: center; pointer-events: none; transition: clip-path 0.5s ease-in-out; }
    #ov-profil-viewer.actif { clip-path: circle(150% at 50% 50%); pointer-events: auto; }
    #ov-doc-viewer { background: rgba(20, 19, 16, 0.98); clip-path: circle(0% at 50% 50%); z-index: 2100; display: flex; flex-direction: column; justify-content: center; align-items: center; pointer-events: none; transition: clip-path 0.5s ease-in-out; }
    #ov-doc-viewer.actif { clip-path: circle(150% at 50% 50%); pointer-events: auto; }

    #ov-profil.actif { clip-path: circle(200vmax at 0% 0%); }
    #ov-messagerie.actif { clip-path: circle(200vmax at 100% 0%); }
    #ov-configuration.actif { clip-path: circle(200vmax at 0% 100%); }
    #ov-actu.actif { clip-path: circle(200vmax at 50% 100%); }

    /* Overlay Contents */
    .content {
      width: 100%;
      max-width: 600px;
      display: flex;
      flex-direction: column;
      gap: 15px;
      opacity: 0;
      transform: translateY(20px);
      transition: opacity 0.5s ease 0.4s, transform 0.5s ease 0.4s;
    }
    .overlay.actif .content {
      opacity: 1;
      transform: translateY(0);
    }
    
    .overlay h1 {
      font-family: Georgia, serif;
      font-style: italic;
      font-size: 24px;
      color: var(--primary);
      text-align: center;
      letter-spacing: 2px;
    }
    .overlay .line {
      width: 60px;
      height: 3px;
      background: var(--primary);
      margin: 5px auto 10px auto;
      border-radius: 2px;
    }
    .overlay p.sub {
      color: var(--text-muted);
      text-align: center;
      font-size: 12px;
      margin-bottom: 10px;
    }

    /* Cards */
    .card {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 8px;
      padding: 15px;
      box-shadow: var(--shadow);
    }
    .card-title {
      font-family: Georgia, serif;
      font-size: 15px;
      font-style: italic;
      color: var(--primary);
      margin-bottom: 10px;
      border-bottom: 1px dashed var(--border);
      padding-bottom: 6px;
      display: flex;
      align-items: center;
      justify-content: space-between;
    }

    /* Forms */
    .form-group {
      display: flex;
      flex-direction: column;
      gap: 5px;
      margin-bottom: 12px;
      text-align: left;
    }
    .form-group label {
      font-size: 11px;
      text-transform: uppercase;
      letter-spacing: 1px;
      color: var(--text-muted);
    }
    .form-group input, .form-group textarea, .form-group select {
      background: rgba(0,0,0,0.3);
      border: 1px solid var(--border);
      border-radius: 6px;
      padding: 10px;
      color: var(--text);
      font-size: 13px;
      outline: none;
      width: 100%;
    }
    .form-group textarea {
      resize: none;
    }
    .form-group input:focus, .form-group textarea:focus, .form-group select:focus {
      border-color: var(--primary);
    }
    
    /* Buttons */
    .btn {
      background: var(--primary);
      color: #121212;
      font-weight: bold;
      border: none;
      border-radius: 6px;
      padding: 10px 16px;
      cursor: pointer;
      font-size: 13px;
      transition: all 0.2s;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      gap: 6px;
    }
    .btn:hover {
      transform: translateY(-1px);
      filter: brightness(1.1);
    }
    .btn-sec {
      background: rgba(255,255,255,0.06);
      color: var(--text);
      border: 1px solid var(--border);
    }
    .btn-danger {
      background: #8c2e2e;
      color: white;
      border: 1px solid rgba(255,255,255,0.1);
    }

    .zone-civvi {
      width: 100%;
      display: flex;
      flex-direction: column;
      padding: 20px 0;
      z-index: 5;
      position: relative;
      flex-grow: 1;
    }
    #prompters {
      display: flex;
      flex-direction: column-reverse;
      gap: 15px;
      padding: 0 90px;
      flex-grow: 1;
    }
    @media (max-width: 500px) {
      #prompters {
        padding: 0 65px !important;
      }
    }

    /* Badge counter for Feuille */
    .badge-feuille {
      position: absolute;
      top: 12px;
      right: 12px;
      background-color: #ef4444;
      color: white;
      font-size: 11px;
      font-weight: bold;
      min-width: 18px;
      height: 18px;
      border-radius: 50%;
      display: none;
      align-items: center;
      justify-content: center;
      padding: 0 4px;
      border: 1px solid var(--surface);
      box-shadow: 0 2px 5px rgba(0,0,0,0.3);
      z-index: 110;
      pointer-events: none;
    }

    /* ESP Message Line styling */
    .esp-line {
      display: flex;
      align-items: center;
      background: var(--surface);
      border-radius: 8px;
      overflow: hidden;
      font-size: 20px;
      border-left: 6px solid;
      border-color: var(--primary);
      box-shadow: var(--shadow);
      min-height: 54px;
      position: relative;
      transition: opacity 0.5s ease;
    }
    .esp-line.expired {
      opacity: 0.35;
      filter: blur(0.5px);
    }
    .cartouche {
      background: var(--surface-high);
      padding: 12px 18px;
      font-weight: bold;
      font-family: Georgia, serif;
      font-style: italic;
      z-index: 10;
      white-space: nowrap;
      border-right: 1px solid var(--border);
    }
    .riviere {
      flex-grow: 1;
      position: relative;
      overflow: hidden;
      display: flex;
      align-items: center;
      white-space: nowrap;
      height: 100%;
      cursor: grab;
    }
    .riviere:active {
      cursor: grabbing;
    }
    .scrolling-text {
      display: inline-block;
      padding-left: 100%;
      padding-right: 100%;
      will-change: transform;
      animation: scroll-left 15s linear infinite;
      user-select: none;
    }
    
    /* Profile & Vote buttons */
    .cv-case-btn {
      background: var(--surface-high);
      border: none;
      border-right: 1px solid var(--border);
      color: var(--text-muted);
      font-size: 18px;
      padding: 0 12px;
      cursor: pointer;
      align-self: stretch;
      display: flex;
      align-items: center;
      justify-content: center;
      z-index: 10;
      transition: background 0.2s, color 0.2s;
    }
    .vote-case-btn {
      background: var(--surface-high);
      border: none;
      border-left: 1px solid var(--border);
      color: var(--text-muted);
      font-size: 18px;
      padding: 0 12px;
      cursor: pointer;
      align-self: stretch;
      display: flex;
      align-items: center;
      justify-content: center;
      z-index: 10;
      transition: background 0.2s, color 0.2s;
    }
    .cv-case-btn:hover, .vote-case-btn:hover {
      background: var(--surface-high);
      color: var(--primary);
    }
    
    .votes-display {
      display: flex;
      gap: 4px;
      align-items: center;
      padding: 0 10px;
      font-size: 12px;
      z-index: 10;
    }
    .vote-badge {
      background: rgba(255,255,255,0.05);
      border: 1px solid rgba(255,255,255,0.1);
      border-radius: 12px;
      padding: 2px 6px;
      color: var(--text-muted);
      white-space: nowrap;
    }

    .ghost-btn-container {
      background: var(--surface-high);
      padding: 0 16px;
      display: flex;
      align-items: center;
      justify-content: center;
      z-index: 10;
      border-left: 1px solid var(--border);
      align-self: stretch;
      cursor: pointer;
      transition: background 0.2s;
    }
    .ghost-btn-container:hover, .ghost-btn-container:active {
      background: rgba(16, 185, 129, 0.15);
    }
    .ghost-btn {
      font-size: 15px;
      pointer-events: none;
      display: flex;
      align-items: center;
      justify-content: center;
    }

    .popover-reactions {
      position: fixed;
      background: var(--surface-high);
      border: 1px solid var(--primary);
      border-radius: 20px;
      padding: 8px 15px;
      display: none;
      gap: 12px;
      z-index: 1000;
      box-shadow: var(--shadow);
    }
    .reaction-emoji-btn {
      background: none;
      border: none;
      font-size: 20px;
      cursor: pointer;
      transition: transform 0.2s;
    }
    .reaction-emoji-btn:hover {
      transform: scale(1.3);
    }



    .radar-list {
      display: flex;
      flex-direction: column;
      gap: 10px;
      max-height: 200px;
      overflow-y: auto;
      border: 1px solid var(--border);
      border-radius: 6px;
      background: rgba(0,0,0,0.2);
    }
    .node-item {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 10px;
      border-bottom: 1px solid rgba(255,255,255,0.05);
    }
    .node-item:last-child { border-bottom: none; }
    .node-info {
      display: flex;
      flex-direction: column;
      text-align: left;
    }
    .node-name {
      font-weight: bold;
      font-family: Georgia, serif;
      font-style: italic;
    }
    .node-ssid {
      font-size: 11px;
      color: var(--text-muted);
    }
    .node-signal {
      display: flex;
      align-items: center;
      gap: 6px;
      font-size: 12px;
    }

    .cv-viewer-card {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 12px;
      width: 90%;
      max-width: 500px;
      padding: 25px;
      box-shadow: var(--shadow);
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 20px;
      position: relative;
    }
    .cv-viewer-avatar {
      width: 110px;
      height: 110px;
      border-radius: 50%;
      object-fit: cover;
      border: 3px solid var(--primary);
      background: var(--surface-high);
    }
    .cv-viewer-text {
      width: 100%;
      text-align: left;
      line-height: 1.6;
      max-height: 250px;
      overflow-y: auto;
      padding-right: 5px;
      font-size: 14px;
    }
    .cv-viewer-text h1 { font-family: Georgia, serif; font-size: 1.6rem; color: var(--primary); margin-bottom: 10px; text-align: left; border-bottom: 1px dashed var(--border); padding-bottom: 5px;}
    .cv-viewer-text h2 { font-family: Georgia, serif; font-size: 1.3rem; color: var(--secondary); margin-bottom: 8px; }
    .cv-viewer-text h3 { font-size: 1.1rem; color: var(--text); margin-bottom: 6px; }
    .cv-viewer-text li { margin-left: 20px; list-style-type: square; }

    #notifications-area {
      position: absolute;
      top: 70px;
      left: 50%;
      transform: translateX(-50%);
      width: 90%;
      max-width: 450px;
      z-index: 1000;
      display: flex;
      flex-direction: column;
      gap: 10px;
    }
    .notification-banner {
      background: rgba(186, 207, 136, 0.15);
      border: 1px solid var(--secondary);
      border-left: 4px solid var(--secondary);
      padding: 10px 15px;
      border-radius: 6px;
      font-size: 12px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      box-shadow: var(--shadow);
      color: var(--text);
    }
    .notification-banner button {
      background: none; border: none; color: inherit; cursor: pointer; font-weight: bold; font-size: 14px;
    }

    .actu-fullscreen-container {
      width: 100%;
      display: flex;
      flex-direction: column;
      gap: 15px;
      padding: 30px 10px;
    }
    .actu-row {
      background: var(--surface-low);
      border: 1px solid var(--border);
      border-radius: 8px;
      padding: 20px;
      font-size: 22px;
      font-family: Georgia, serif;
      font-style: italic;
      text-align: center;
      line-height: 1.5;
    }

    @keyframes defilementD-G {
      0% { left: 100%; transform: translateX(0); }
      100% { left: 0; transform: translateX(-100%); }
    }
    @keyframes scroll-left {
      0% { transform: translateX(0); }
      100% { transform: translateX(-100%); }
    }

    @media (max-width: 1024px) {
      .control-bar {
        padding: 10px 90px !important;
      }
      .control-bar .left-controls {
        display: flex !important;
        flex-direction: row !important;
        width: 100%;
        min-width: 0 !important;
        height: auto !important;
        justify-content: space-between;
        align-items: center;
        gap: 15px !important;
        border-top: 1px dashed var(--border);
        padding-top: 8px;
        margin-top: 4px;
      }
      .control-bar .left-controls > div {
        flex: 1;
      }
      .control-bar .msg-box {
        width: 100%;
        min-height: 54px !important;
        margin: 0 !important;
      }
    }

    @media (max-width: 500px) {
      .btn-coin {
        width: 54px !important;
        height: 54px !important;
      }
      .c1 { border-radius: 0 0 54px 0 !important; }
      .c3 { border-radius: 0 0 0 54px !important; }
      .c11 { border-radius: 0 54px 0 0 !important; }
      .c12 { border-radius: 54px 0 0 0 !important; }
      .btn-coin svg { width: 20px !important; height: 20px !important; margin: 0 !important; }
      .c1 svg { margin-right: 10px !important; margin-bottom: 10px !important; }
      .c3 svg { margin-left: 10px !important; margin-bottom: 10px !important; }
      .c11 svg { margin-right: 10px !important; margin-top: 10px !important; }
      .c12 svg { margin-left: 10px !important; margin-top: 10px !important; }
      
      .top-bar { padding: 0 65px !important; }
      .bottom-bar { padding: 0 65px !important; }
      .control-bar { padding: 10px 65px !important; }
    }

    /* Splash Screen Styling */
    #splash-screen {
      position: fixed;
      top: 0; left: 0;
      width: 100%; height: 100%;
      background-color: var(--bg);
      background-image: url("data:image/svg+xml,%3Csvg width='80' height='80' xmlns='http://www.w3.org/2000/svg'%3E%3Cfilter id='noise'%3E%3CfeTurbulence type='fractalNoise' baseFrequency='0.8' numOctaves='3' stitchTiles='stitch'/%3E%3C/filter%3E%3Crect width='100%25' height='100%25' filter='url(%23noise)' opacity='0.04'/%3E%3C/svg%3E");
      z-index: 9999;
      display: flex;
      flex-direction: column;
      justify-content: center;
      align-items: center;
      transition: opacity 0.8s ease-out, visibility 0.8s;
      pointer-events: all;
    }
    #splash-screen.hidden {
      opacity: 0;
      visibility: hidden;
      pointer-events: none;
    }
    .splash-container {
      text-align: center;
      position: relative;
      z-index: 10;
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 20px;
      padding: 40px;
    }
    .splash-logo {
      font-family: Georgia, serif;
      font-style: italic;
      color: var(--primary);
      font-size: 54px;
      letter-spacing: 6px;
      margin-bottom: 5px;
      animation: pulse 2.5s infinite ease-in-out;
    }
    .splash-scan-line {
      width: 100%;
      height: 2px;
      background: linear-gradient(90deg, transparent, var(--primary), transparent);
      position: absolute;
      top: 0;
      left: 0;
      animation: scan-vertical 3s linear infinite;
    }
    .splash-status {
      font-size: 13px;
      text-transform: uppercase;
      letter-spacing: 2px;
      color: var(--text);
      font-weight: 500;
    }
    .splash-counter {
      font-size: 16px;
      font-weight: bold;
      color: var(--primary);
      background: rgba(217, 149, 43, 0.1);
      border: 1.5px solid var(--primary);
      padding: 10px 24px;
      border-radius: 25px;
      letter-spacing: 0.5px;
      text-shadow: 0 0 10px rgba(217, 149, 43, 0.3);
      box-shadow: 0 0 15px rgba(217, 149, 43, 0.15);
    }
    #received-graffitis-list {
      margin: 0 auto !important;
    }
    @media (max-width: 480px) {
      #msg {
        font-size: 16px !important;
      }
    }
    /* Salon Mode overrides to hide P2P specific functions */
    body.salon-active .btn-coin,
    body.salon-active .btn-retour {
      display: none !important;
    }
    body.salon-active #config-mode-card {
      display: none !important;
    }
    body.salon-mode .sort-box {
      display: none !important;
    }
    body.salon-mode .left-controls button {
      display: none !important;
    }
    body.salon-mode .left-controls {
      justify-content: center !important;
      border-top: 1px dashed var(--border);
      padding-top: 8px;
      margin-top: 4px;
    }
    body.salon-mode .speed-box {
      width: 100% !important;
      max-width: 300px !important;
      display: flex !important;
      flex-direction: column !important;
      align-items: center !important;
      gap: 2px !important;
    }
    body.salon-mode .cv-case-btn {
      display: none !important;
    }
    body.salon-mode .vote-case-btn {
      display: none !important;
    }
    body.salon-mode .votes-display {
      display: none !important;
    }
    body.salon-mode .ghost-btn-container {
      display: none !important;
    }
    @keyframes scan-vertical {
      0% { top: 0%; }
      100% { top: 100%; }
    }
    @keyframes pulse {
      0% { opacity: 0.7; transform: scale(0.98); }
      50% { opacity: 1; transform: scale(1); text-shadow: 0 0 15px rgba(217, 149, 43, 0.35); }
      100% { opacity: 0.7; transform: scale(0.98); }
    }

    /* Mode Selection Screen Styling */
    #mode-selection-screen {
      position: fixed;
      top: 0; left: 0;
      width: 100%; height: 100%;
      background-color: var(--bg);
      background-image: url("data:image/svg+xml,%3Csvg width='80' height='80' xmlns='http://www.w3.org/2000/svg'%3E%3Cfilter id='noise'%3E%3CfeTurbulence type='fractalNoise' baseFrequency='0.8' numOctaves='3' stitchTiles='stitch'/%3E%3C/filter%3E%3Crect width='100%25' height='100%25' filter='url(%23noise)' opacity='0.04'/%3E%3C/svg%3E");
      z-index: 9998;
      display: flex;
      flex-direction: column;
      justify-content: center;
      align-items: center;
      transition: opacity 0.5s ease-in-out, visibility 0.5s;
    }
    #mode-selection-screen.hidden {
      display: none !important;
      opacity: 0 !important;
      visibility: hidden !important;
      pointer-events: none !important;
    }
    .mode-container {
      text-align: center;
      max-width: 650px;
      width: 90%;
      padding: 30px;
      background: var(--surface-low);
      border: 1px solid var(--border);
      border-radius: 12px;
      box-shadow: var(--shadow);
      display: flex;
      flex-direction: column;
      gap: 15px;
    }
    .mode-title {
      font-family: Georgia, serif;
      font-style: italic;
      color: var(--primary);
      font-size: 28px;
      letter-spacing: 2px;
    }
    .mode-subtitle {
      font-size: 13px;
      color: var(--text-muted);
      margin-bottom: 10px;
    }
    .mode-choices {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 20px;
      margin-top: 10px;
    }
    @media (max-width: 600px) {
      .mode-choices {
        grid-template-columns: 1fr;
      }
    }
    .mode-card {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 8px;
      padding: 20px;
      cursor: pointer;
      transition: all 0.3s;
      position: relative;
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 10px;
      text-align: center;
    }
    .mode-card:hover {
      border-color: var(--primary);
      box-shadow: 0 0 15px rgba(217, 149, 43, 0.2);
      transform: translateY(-2px);
    }
    .mode-card-icon {
      font-size: 32px;
    }
    .mode-card-title {
      font-family: Georgia, serif;
      font-style: italic;
      font-size: 18px;
      color: var(--primary);
    }
    .mode-card-desc {
      font-size: 11px;
      color: var(--text-muted);
      line-height: 1.5;
    }
    .mode-badge-active {
      position: absolute;
      top: 10px;
      right: 10px;
      background: rgba(16, 185, 129, 0.15);
      border: 1px solid #10b981;
      color: #10b981;
      font-size: 9px;
      font-weight: bold;
      padding: 2px 6px;
      border-radius: 10px;
      text-transform: uppercase;
      display: none;
    }
    .mode-card.current {
      border-color: var(--primary);
      background: rgba(217, 149, 43, 0.05);
    }
    .mode-card.current .mode-badge-active {
      display: block;
    }
  </style>
</head>
<body>

<div id="splash-screen" class="hidden" style="position:fixed; top:0; left:0; width:100vw; height:100vh; height:100dvh; background:var(--bg); z-index:9999; display:none; justify-content:center; align-items:center;">
    <div class="splash-scan-line"></div>
    <div class="splash-container">
        <div class="splash-logo">CIVVI</div>
        <div class="splash-status" id="splash-status-text" style="font-size:15px; letter-spacing:3px;">Scan du réseau en cours...</div>
        <div class="splash-counter" id="splash-counter-text" style="margin-top:15px; font-weight:bold; color:var(--primary);">Recherche de modules actifs...</div>
    </div>
</div>



    <!-- écran "7" b : fenêtre de rédaction et de validation du pseudonyme pour les visiteurs -->
    <div id="salon-username-modal" class="overlay" style="z-index: 5000; background: rgba(10, 9, 8, 0.98); justify-content: center; align-items: center; display: none;">
        <div class="card" style="width: 90%; max-width: 400px; text-align: center; padding: 25px; border: 1.5px solid var(--primary); box-shadow: 0 0 25px rgba(217, 149, 43, 0.25);">
            <h1 style="font-family: Georgia, serif; color: var(--primary); font-style: italic; font-size: 24px; margin-bottom: 5px;">CIVVI - Salon</h1>
            <div class="line" style="margin: 10px auto;"></div>
            <p style="font-size: 12px; color: var(--text-muted); margin-bottom: 20px;">Veuillez choisir un pseudonyme pour participer au salon de discussion</p>
            
            <div class="form-group" style="text-align: left; margin-bottom: 20px;">
                <label style="color: var(--primary); font-size: 11px; font-weight: bold; text-transform: uppercase;">Votre Pseudonyme :</label>
                <input type="text" id="salon-modal-username-input" placeholder="Entrez votre pseudo..." maxlength="20" style="padding: 10px; font-size: 14px; margin-top: 6px; background: rgba(0,0,0,0.3); border: 1px solid var(--border); color: var(--text); border-radius: 6px; outline: none; width: 100%; text-align: center;">
            </div>
            
            <button class="btn" onclick="confirmSalonUsername()" style="width: 100%; padding: 10px; font-size: 13px; font-weight: bold; text-transform: uppercase; cursor: pointer;">Rejoindre la discussion</button>
        </div>
    </div>

<!-- écran "1" : l'interface principale -->
<div class="esp-interface">
    
        <!-- ecran "0" : écran de démarrage -->
    <div id="screen-demarrage" style="position:fixed; top:0; left:0; width:100vw; height:100vh; height:100dvh; background:var(--bg); z-index:9999; display:flex; flex-direction:column; justify-content:center; align-items:center; transition: opacity 0.5s ease-in-out, visibility 0.5s;">
        <!-- "0.1" : logo -->
        <img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAPAAAACJCAIAAACdGiLMAAAAA3NCSVQICAjb4U/gAAAAGXRFWHRTb2Z0d2FyZQBnbm9tZS1zY3JlZW5zaG907wO/PgAAIABJREFUeJxsvNmObFlyJWbDns7gU0z3Zt4cqypRJAtoUGqQRDcBQmo9qZ/6n/Qk/Yj6F9QPAgE+qBtFsThXZ1VmVt7Km3eMyd3PtCezfji3Bkp9EAiccI/YHm7Hztpmay1z/D/+9//NWb/EmZl32+0S44sXLzabTd9tHh7vu667uro6n87ffffdRx999OrVy9A0wzB++smn8zzfPzzs9/vrq+t3795N0/jhs2eE+PDwWGs9HPYvXrw4D8Nf/MVf3N3efvXVV59+9tnV5eWrV29O5+Nhf3DOvX37tm3bp0+fvH7z5uXLV88+/PDq+uru7m4cxsPhYK358stffvjsA8N2GM9E9Oknn7x8+UpBp3F6+vRJzvndu9snT26MMd9886umCZ9++uk4js+f/3q33z179myZly+//PLf/Nt/8+LFi7u7u8P+8Nlnn/3zz/85xfTkyZOmaV69eplzubq+LqXM83RzfXN1dfX23dsX3333gx/+8NWrV6rqrL26unr9+rUq7Pc759ybN2/arn9yc30+Dy9efPf06Qdd3/3sZz97+uTph88+fHw4Pn/+/PPPP/vw2bO/+qu/+uEPfvDRRx/f3b37+3/4h5/80R999tnnv/zql1999fVP/uiPYoxv3775+OOPt9vd/f39d9/9+pNPPn327Nnf/t3fvnv37t//+3//93/3d69ev3n20bMf/8Ef/Of//P/c3d3/23/7b6y1//SP/0RMn332+el0fvPm9QdPnx4uLr7+6qvvX37/H/7Df/jLv/zLWqTW8j/9z//u22+/ffXy5ccff7Lb77777rv9br/f74dhePXq1fX19Xa7ff78ea31cDiEEP76r//6iy+++NM//dOf/vSnv/r22x98/vkf/uEf/l//6T8Z5j/7sz97++7tV7/8+gc/+Pzy6vLN6zen0/nm5om19uc//3nbtn/8x3/88PDwl3/5l3/+53++3W6//PLLh4eHf/e//LtfffP1q1evPv7k4x//+Mf/8A//8Pz5r//0T/6k6/qf/exvSi0/+uGP5mX+5utvPvroo5/85Cc/+9ufffXLr/7kT/7EOffV11/V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" style="width:160px; height:auto; margin-bottom:20px; filter:drop-shadow(0 0 15px rgba(217, 149, 43, 0.4));">
        <div style="font-family: Georgia, serif; color: var(--primary); font-style: italic; font-size: 20px; margin-bottom: 30px;">CIVVI</div>
        <!-- "0.2" : bouton pour accéder à l'interface principale -->
        <button class="btn" onclick="entrerInterface()" style="padding: 10px 24px; font-size: 13px; font-weight: bold; text-transform: uppercase; cursor: pointer; border-radius: 8px;">Accéder à l'interface principale</button>
    </div>
    <div class="lignes-fond"></div>


    <!-- "1.1" : pour accéder à l'écran "profil" (point sur fond bleu) -->
    <div id="btn-profil" class="btn-coin c1" onclick="ouvrirOverlay('ov-profil')" title="profil">
        <svg viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
            <circle cx="12" cy="12" r="6" fill="#ffffff" />
        </svg>
    </div>

    <!-- "1.2" : pour accéder à l'écran "messagerie" (segment sur fond jaune, avec pastille indiquant le nombre de messages non lus) -->
    <div id="btn-messagerie" class="btn-coin c3" onclick="ouvrirOverlay('ov-messagerie')" title="messagerie">
        <svg viewBox="0 0 24 24" fill="none" stroke="#1e293b" stroke-width="3" stroke-linecap="round" stroke-linejoin="round">
            <line x1="5" y1="19" x2="19" y2="5" />
        </svg>
    </div>

    <!-- "1.3" : pour accéder à l'écran "configuration" (triangle sur fond rouge) -->
    <div id="btn-configuration" class="btn-coin c11" onclick="ouvrirOverlay('ov-configuration')" title="configuration">
        <svg viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="2.5" stroke-linejoin="round">
            <polygon points="12,4 4,18 20,18" fill="#ffffff" />
        </svg>
    </div>

    <!-- "1.4" : pour accéder à l'écran "acceuil" (carré sur fond violet) -->
    <div id="btn-accueil" class="btn-coin c12" onclick="fermerOverlays(true)" title="chaine humaine">
        <svg viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
            <rect x="5" y="5" width="14" height="14" fill="#ffffff" />
        </svg>
    </div>

    <!-- "1.5" : la première ligne (tout en haut) : "ligne d'information réseau" -->
    <div class="top-bar" style="position:relative;">
        <!-- écran "7" a : bouton de fermeture du salon pour l'hébergeur -->
        <button id="btn-admin-p2p" class="btn" style="display:none; position:absolute; right:15px; top:50%; transform:translateY(-50%); font-size:11px; padding:4px 8px; z-index:100; background:#dc2626; color:white; border-radius:4px; border:1px solid var(--border);" onclick="switchBackToP2P()">📡 P2P</button>
        <div style="display:grid; grid-template-columns: 1fr auto 1fr; align-items:center; width:100%;">
            <div class="header-title" id="header-user" style="text-align:left; font-size:12px; letter-spacing:1px; white-space:nowrap; overflow:hidden; text-overflow:ellipsis;">Nom de l'utilisateur : Chargement...</div>
            <div style="display:flex; justify-content:center;">
                <span class="badge" id="module-count" style="font-size:11px;">0 Module(s)</span>
            </div>
            <div id="header-ssid" style="color: var(--text-muted); font-size:11px; font-style:italic; text-align:right; white-space:nowrap; overflow:hidden; text-overflow:ellipsis;">Nom du réseau : Chargement...</div>
        </div>
        <!-- "1.6" : la deuxième ligne : "ligne du message actuellement diffusé par l'utilisateur" -->
        <div class="my-thought-marquee-container">
            <div id="my-thought-marquee" class="my-thought-marquee">Message diffusé : Chargement...</div>
        </div>
    </div>

    <!-- "1.7" : la zone centrale : lignes "cibi-textuelle" -->
    <div class="zone-civvi">
        <div id="notifications-area"></div>
        <div id="prompters">
        </div>
    </div>

    <!-- "1.8" : la ligne d'actualité du réseau -->
    <div class="bottom-bar" onclick="ouvrirOverlay('ov-actu')">
        <div class="marquee-actu" id="marquee-actu-text">Chargement de l'actualité réseau...</div>
    </div>

    <!-- "1.10" : la ligne de gestion des messages -->
    <div class="control-bar">
        <!-- "1.9" : la ligne de rédaction des messages -->
        <div class="msg-box" style="display:flex; flex-direction:column; background:rgba(255,255,255,0.07); border:1.5px solid var(--primary); border-radius:6px; padding:6px 12px; position:relative; min-height:54px; justify-content:center; width:100%;">
            <textarea id="msg" placeholder="Écrivez votre message ici..." maxlength="3500" style="background:none; border:none; color:var(--text); font-size:13px; outline:none; width:100%; height:36px; resize:none; font-family:inherit; padding-right:80px; line-height:1.4;"></textarea>
            <span id="char-counter" style="font-size:10px; color:var(--text-muted); position:absolute; right:15px; bottom:8px;">0 / 3500</span>
        </div>
        <div class="left-controls" style="position:relative; display:flex; flex-direction:row; align-items:center; justify-content:space-between; width:100%;">
            <!-- "1.10.1" : "tri" -->
            <div class="sort-box" style="display:flex; align-items:center; gap:8px;">
                <span style="font-size:9px; text-transform:uppercase; color:var(--text-muted); letter-spacing:0.5px; white-space:nowrap;">Tri :</span>
                <select id="sort-select" onchange="changeSortMode(this.value)" style="background: rgba(0,0,0,0.3); border: 1px solid var(--border); color: var(--text); border-radius: 4px; padding: 2px 4px; font-size: 11px; outline: none; cursor: pointer; font-family: inherit; width: 120px;">
                  <!-- "1.10.1.1" : favoris en premier -->
                  <option value="fav_first">Favoris en premier</option>
                  <!-- "1.10.1.2" : modules les plus proches -->
                  <option value="rssi_desc">Modules les plus proches</option>
                  <!-- "1.10.1.3" : modules les plus lointains -->
                  <option value="rssi_asc">Modules les plus lointains</option>
                  <!-- "1.10.1.4" : nom (a-z) -->
                  <option value="alpha_asc">Nom (A-Z)</option>
                  <!-- "1.10.1.5" : nom (z-a) -->
                  <option value="alpha_desc">Nom (Z-A)</option>
                </select>
            </div>
            <!-- "1.10.2" : "actualisation" -->
            <button onclick="update()" class="btn" style="position:absolute; left:50%; transform:translateX(-50%); padding: 6px; font-size: 18px; display: flex; align-items: center; justify-content: center; border-radius: 50%; cursor: pointer; border: 2px solid var(--primary); background: var(--surface-high); color: var(--text); box-shadow: 0 4px 10px rgba(0,0,0,0.3); width: 42px; height: 42px; z-index: 10;" title="Actualiser">
              <span>🔄</span>
            </button>
            <!-- "1.10.3" : "vitesse de défilement" -->
            <div class="speed-box" style="display:flex; flex-direction:column; align-items:center; gap:2px; width:100%; max-width:180px; justify-content:flex-end; text-align:right;">
                <input type="range" id="speed-slider" min="0.2" max="2" step="0.1" value="1.0" style="width:100%; height:6px; accent-color:var(--primary); cursor:pointer; margin:0;">
                <span style="font-size:8px; text-transform:uppercase; color:var(--text-muted); letter-spacing:0.5px; white-space:nowrap;">Vit: <span id="speed-value">1.0</span>x</span>
            </div>
        </div>
    </div>



    <!-- écran "2" : profil -->
    <div id="ov-profil" class="overlay">
        <div class="content">
            <h1>Profil</h1>
            <div class="line"></div>
            
            <!-- "2.1" : personnalisation de l'avatar -->
            <div class="card">
              <div class="card-title">Personnalisation de l'Avatar</div>
              
              <div class="avatar-editor-layout">
                <!-- Gauche: Aperçu visuel -->
                <div class="avatar-preview-box">
                  <div class="avatar-head" id="part-head"></div>
                  <div class="avatar-torso" id="part-torso">
                    <div class="pattern-overlay" id="pattern-torso"></div>
                  </div>
                  <div class="avatar-legs" id="part-legs">
                    <div class="pattern-overlay" id="pattern-legs"></div>
                    <div class="avatar-leg-left"></div>
                    <div class="avatar-leg-right"></div>
                  </div>
                  <div class="avatar-feet" id="part-feet">
                    <div class="avatar-foot" id="part-foot-l"></div>
                    <div class="avatar-foot" id="part-foot-r"></div>
                  </div>
                </div>
                
                <!-- Droite: Contrôles -->
                <div class="avatar-controls-column">
                  <!-- "2.1.1" : cheveux / tête -->
                  <div class="form-group">
                    <label>👨‍🦱 Cheveux / Tête :</label>
                    <div class="color-swatches-row">
                      <button class="color-swatch-btn" onclick="selectAvatarColor('head', 'transparent')" style="background:#000000; border:2px solid #ef4444; display:flex; align-items:center; justify-content:center; color:#ef4444; font-size:12px; font-weight:bold;" title="Chauve">❌</button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('head', '#ffffff')" style="background:#ffffff;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('head', '#000000')" style="background:#000000;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('head', '#3d2314')" style="background:#3d2314;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('head', '#634439')" style="background:#634439;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('head', '#e6be8a')" style="background:#e6be8a;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('head', '#b55239')" style="background:#b55239;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('head', '#9ca3af')" style="background:#9ca3af;"></button>
                    </div>
                  </div>
                  
                  <!-- "2.1.2" : buste -->
                  <div class="form-group">
                    <label>👕 Buste (Vêtement) :</label>
                    <div class="color-swatches-row" style="margin-bottom:6px;">
                      <button class="color-swatch-btn" onclick="selectAvatarColor('torso', 'transparent')" style="background:#000000; border:2px solid #ef4444; display:flex; align-items:center; justify-content:center; color:#ef4444; font-size:12px; font-weight:bold;" title="Pas de couleur">❌</button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('torso', '#ffffff')" style="background:#ffffff;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('torso', '#ef4444')" style="background:#ef4444;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('torso', '#eab308')" style="background:#eab308;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('torso', '#22c55e')" style="background:#22c55e;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('torso', '#3b82f6')" style="background:#3b82f6;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('torso', '#ec4899')" style="background:#ec4899;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('torso', '#a855f7')" style="background:#a855f7;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('torso', '#808080')" style="background:#808080;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('torso', '#1f2937')" style="background:#1f2937;"></button>
                    </div>
                    <div class="pattern-btns-row">
                      <span style="font-size:10px; color:var(--text-muted); text-transform:uppercase; font-weight:bold; letter-spacing:0.5px;">Motif :</span>
                      <button class="pattern-select-btn" onclick="selectAvatarPattern('torso', '')" title="Uni"><span style="width:10px; height:10px; background:#9ca3af; border-radius:1px;"></span></button>
                      <button class="pattern-select-btn" onclick="selectAvatarPattern('torso', 'pattern-stripes')" title="Rayures"><div style="width:12px; height:12px;" class="pattern-stripes opacity-40"></div></button>
                      <button class="pattern-select-btn" onclick="selectAvatarPattern('torso', 'pattern-dots')" title="Pois"><div style="width:12px; height:12px;" class="pattern-dots opacity-40"></div></button>
                      <button class="pattern-select-btn" onclick="selectAvatarPattern('torso', 'pattern-geometric')" title="Géométrique"><div style="width:12px; height:12px;" class="pattern-geometric opacity-40"></div></button>
                    </div>
                  </div>

                  <!-- "2.1.3" : jambes -->
                  <div class="form-group">
                    <label>👖 Jambes (Pantalon) :</label>
                    <div class="color-swatches-row" style="margin-bottom:6px;">
                      <button class="color-swatch-btn" onclick="selectAvatarColor('legs', 'transparent')" style="background:#000000; border:2px solid #ef4444; display:flex; align-items:center; justify-content:center; color:#ef4444; font-size:12px; font-weight:bold;" title="Pas de couleur">❌</button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('legs', '#ffffff')" style="background:#ffffff;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('legs', '#ef4444')" style="background:#ef4444;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('legs', '#eab308')" style="background:#eab308;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('legs', '#22c55e')" style="background:#22c55e;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('legs', '#3b82f6')" style="background:#3b82f6;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('legs', '#ec4899')" style="background:#ec4899;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('legs', '#a855f7')" style="background:#a855f7;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('legs', '#808080')" style="background:#808080;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('legs', '#1f2937')" style="background:#1f2937;"></button>
                    </div>
                    <div class="pattern-btns-row">
                      <span style="font-size:10px; color:var(--text-muted); text-transform:uppercase; font-weight:bold; letter-spacing:0.5px;">Motif :</span>
                      <button class="pattern-select-btn" onclick="selectAvatarPattern('legs', '')" title="Uni"><span style="width:10px; height:10px; background:#9ca3af; border-radius:1px;"></span></button>
                      <button class="pattern-select-btn" onclick="selectAvatarPattern('legs', 'pattern-stripes')" title="Rayures"><div style="width:12px; height:12px;" class="pattern-stripes opacity-40"></div></button>
                      <button class="pattern-select-btn" onclick="selectAvatarPattern('legs', 'pattern-dots')" title="Pois"><div style="width:12px; height:12px;" class="pattern-dots opacity-40"></div></button>
                      <button class="pattern-select-btn" onclick="selectAvatarPattern('legs', 'pattern-geometric')" title="Géométrique"><div style="width:12px; height:12px;" class="pattern-geometric opacity-40"></div></button>
                    </div>
                  </div>

                  <!-- "2.1.4" : chaussures -->
                  <div class="form-group">
                    <label>👟 Chaussures :</label>
                    <div class="color-swatches-row">
                      <button class="color-swatch-btn" onclick="selectAvatarColor('feet', 'transparent')" style="background:#000000; border:2px solid #ef4444; display:flex; align-items:center; justify-content:center; color:#ef4444; font-size:12px; font-weight:bold;" title="Pas de couleur">❌</button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('feet', '#ffffff')" style="background:#ffffff;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('feet', '#ef4444')" style="background:#ef4444;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('feet', '#eab308')" style="background:#eab308;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('feet', '#22c55e')" style="background:#22c55e;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('feet', '#3b82f6')" style="background:#3b82f6;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('feet', '#ec4899')" style="background:#ec4899;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('feet', '#a855f7')" style="background:#a855f7;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('feet', '#808080')" style="background:#808080;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('feet', '#1f2937')" style="background:#1f2937;"></button>
                    </div>
                  </div>
                </div>
              </div>

              <!-- "2.1.5" : buttons "enregistrer le profil" et "réinitialiser" -->
              <div style="display:flex; gap:10px; margin-top:20px;">
                <button class="btn" onclick="saveStructuredProfile()" style="flex:1;">Enregistrer le profil</button>
                <button class="btn btn-sec" onclick="resetAvatar()" style="flex:1;">Réinitialiser</button>
              </div>
            </div>

            <!-- "2.2" : centres d'intérêt -->
            <div class="card" style="margin-top:20px; text-align:left;">
              <div class="card-title">Centres d'intérêt</div>
              
              <!-- "2.2.1" : boutons de mise en forme du texte -->
              <div class="markdown-toolbar" style="display:flex; gap:6px; background:rgba(0,0,0,0.4); padding:6px; border-radius:6px 6px 0 0; border:1px solid var(--border); border-bottom:none; flex-wrap:wrap; align-items:center;">
                <button class="btn btn-sec" type="button" style="padding:2px 8px; font-size:11px; min-width:24px;" onclick="insertMarkdown('bold')" title="Gras (Bold)"><strong>G</strong></button>
                <button class="btn btn-sec" type="button" style="padding:2px 8px; font-size:11px; min-width:24px;" onclick="insertMarkdown('italic')" title="Italique (Italic)"><em>I</em></button>
                <button class="btn btn-sec" type="button" style="padding:2px 8px; font-size:11px; min-width:24px;" onclick="insertMarkdown('underline')" title="Souligné (Underline)"><u>U</u></button>
                <button class="btn btn-sec" type="button" style="padding:2px 8px; font-size:11px; min-width:32px;" onclick="insertMarkdown('h1')" title="Titre 1">H1</button>
                <button class="btn btn-sec" type="button" style="padding:2px 8px; font-size:11px; min-width:32px;" onclick="insertMarkdown('h2')" title="Titre 2">H2</button>
                <button class="btn btn-sec" type="button" style="padding:2px 8px; font-size:11px; min-width:24px;" onclick="insertMarkdown('list')" title="Liste">-</button>
                <button class="btn btn-sec" type="button" style="padding:2px 8px; font-size:11px; min-width:24px;" onclick="insertMarkdown('quote')" title="Citation">&gt;</button>
                <button class="btn btn-sec" type="button" style="padding:2px 8px; font-size:11px; min-width:45px;" onclick="insertMarkdown('link')" title="Lien">[Lien]</button>
              </div>
              
              <!-- "2.2.2" : zone de rédaction -->
              <div class="form-group" style="margin-top:0;">
                <textarea id="cv-markdown-editor" style="height:150px; font-family:monospace; background:rgba(0,0,0,0.3); border:1px solid var(--border); border-top:none; border-radius:0 0 6px 6px; color:var(--text); padding:8px; width:100%; resize:vertical; font-size:12px; outline:none;" placeholder="# Mes Centres d'intérêt&#10;&#10;- Lecture&#10;- Électronique..."></textarea>
              </div>
              <!-- "2.2.3" : "enregistrement" -->
              <button class="btn" onclick="saveMarkdownCV()" style="width:100%; margin-top:8px;">Enregistrer le document</button>
            </div>


            <!-- "2.3" : favoris -->
            <div class="card" style="margin-top:20px; text-align:left;">
              <div class="card-title">Favoris</div>
              <div id="favorites-list-container" style="display:flex; flex-direction:column; gap:6px; max-height:150px; overflow-y:auto; font-size:12px; margin-bottom:10px;">
              </div>
            </div>

            <!-- "2.4" : historique des diffusions & emoticon reçus & suppression du message de l'historique + bouton "effacer tout l'historique" -->
            <div class="card" style="margin-top:20px; text-align:left;">
              <div class="card-title">Historique des diffusions</div>
              <div id="broadcast-history-list" style="display:flex; flex-direction:column; gap:8px; max-height:180px; overflow-y:auto; font-size:12px; margin-bottom:12px;">
              </div>
              <button class="btn btn-sec btn-danger" style="width:100%; padding:6px; font-size:11px;" onclick="clearHistory()">Effacer Historique</button>
            </div>
        </div>
    </div>

    <!-- écran "3" : messagerie -->
    <div id="ov-messagerie" class="overlay">
        <div class="content" style="text-align:center; display:flex; flex-direction:column; align-items:center;">
            <!-- "3.1" : titre de la page -->
            <h1>Messagerie</h1>
            <div class="line"></div>
            <p class="sub" style="margin-bottom:20px;">messages reçus</p>
            
            <!-- "3.2" : messages avec bouton de suppression, fonction de réponse et info sur l'état de connection -->
            <div id="received-graffitis-list" style="display:flex; flex-direction:column; gap:12px; max-height:400px; overflow-y:auto; width:100%; max-width:500px; padding-right:10px; margin:0 auto;">
            </div>
        </div>
    </div>

    <!-- écran "4" : configuration -->
    <div id="ov-configuration" class="overlay">
        <div class="content">
            <h1>Configuration</h1>
            <div class="line"></div>
            <p class="sub">Options Réseau & Configuration</p>
            
            <!-- "4.1" : espace disponible -->
            <div class="card">
              <div class="card-title">Espace disponible</div>
              <div id="fs-space-info" style="font-size:12px; color:var(--text-muted); line-height:1.5;">Chargement de l'espace...</div>
            </div>

            <!-- "4.2" : configuration réseau -->
            <div class="card">
              <div class="card-title">Configuration réseau</div>
              <!-- "4.2.1" : nom du réseau -->
              <div class="form-group">
                <label>Nom du réseau (SSID) :</label>
                <input type="text" id="config-ssid" placeholder="Ex: Relais-Civvi">
              </div>
              <!-- "4.2.2" : mot de passe -->
              <div class="form-group">
                <label>Mot de passe (Vide = Réseau ouvert) :</label>
                <input type="password" id="config-pwd" placeholder="Optionnel">
              </div>
              <!-- "4.2.3" : nom d'utilisateur -->
              <div class="form-group">
                <label>Nom d'utilisateur affiché :</label>
                <input type="text" id="config-username" placeholder="Votre pseudonyme">
              </div>
              <!-- "4.2.4" : message par défaut au rallumage -->
              <div class="form-group">
                <label>Message par défaut au rallumage :</label>
                <textarea id="config-reboot-msg" style="height:50px;" placeholder="Message de reboot..."></textarea>
              </div>
              <!-- "4.2.5" : rediffuser le dernier message au redémarrage -->
              <div class="form-group" style="margin-top:10px;">
                <label style="display:flex; align-items:center; gap:8px; cursor:pointer; user-select:none; font-size:13px;">
                  <input type="checkbox" id="config-rediffuse-last" style="width:18px; height:18px; accent-color:var(--primary); cursor:pointer;">
                  <span>Rediffuser le dernier message envoyé au démarrage</span>
                </label>
              </div>
            </div>

            <!-- "4.3" : passage en mode salon -->
            <div class="card">
              <div class="card-title">Passage en mode salon</div>
              <p style="font-size:11px; color:var(--text-muted); margin-bottom:12px;">Activez le mode Salon de discussion décentralisé sur ce module. Les participants pourront clavarder sur votre réseau.</p>
              <button class="btn btn-sec" style="width:100%; justify-content:center; font-size:11px; background:rgba(217, 149, 43, 0.1);" onclick="switchToSalonMode()">💬 Activer le Mode Salon</button>
            </div>

            <!-- "4.4" : "sauvegarder et redemarrer" -->
            <div class="card">
              <div class="card-title">Sauvegarder et redémarrer</div>
              <button class="btn" style="width:100%; justify-content:center;" onclick="saveConfiguration()">Sauvegarder et redémarrer</button>
            </div>
        </div>
    </div>

    <!-- écran "6" : actualité pleine écran (en mode P2P) & écran "8" : actualité mode salon en plein écran (en mode Salon) -->
    <div id="ov-actu" class="overlay" onclick="fermerOverlays()" style="background: rgba(10, 9, 8, 0.98); justify-content: center; align-items: center; overflow: hidden; padding: 0; cursor: pointer;">
        <div id="ov-actu-marquee" style="width: auto; min-width: 100%; white-space: nowrap; font-family: Georgia, serif; font-style: italic; font-size: 4.8vw; color: var(--primary); will-change: transform; display: inline-block; text-shadow: 0 0 25px rgba(217, 149, 43, 0.5);">
            En attente d'actualité...
        </div>
    </div>

    <div id="ov-doc-viewer" class="overlay">
        <div class="cv-viewer-card">
          <div class="card-title" id="doc-viewer-title">Centres d'intérêt</div>
          <button style="position:absolute; top:12px; right:15px; background:none; border:none; color:var(--text-muted); cursor:pointer; font-weight:bold; font-size:16px;" onclick="document.getElementById('ov-doc-viewer').classList.remove('actif')">✖</button>
          
          <div class="cv-viewer-text" id="doc-viewer-content" style="max-height:350px; overflow-y:auto; text-align:left; padding:10px; font-family:inherit;">
          </div>
        </div>
    </div>







    <div id="ov-profil-viewer" class="overlay">
        <div class="cv-viewer-card">
          <div class="card-title">Profil Civvi</div>
          <button style="position:absolute; top:12px; right:15px; background:none; border:none; color:var(--text-muted); cursor:pointer; font-weight:bold; font-size:16px;" onclick="document.getElementById('ov-profil-viewer').classList.remove('actif')">✖</button>
          
          <div class="cv-viewer-text" id="cv-viewer-content">
          </div>
        </div>
    </div>

    <!-- écran "5" : accueil (ouverture de la page "chaine humaine" depuis l'interface principale, et retour principal) -->
    <div id="ov-lorem" class="overlay" style="background:#321463;">
        <div class="content" style="text-align:center; display:flex; flex-direction:column; align-items:center;">
            <h1>chaine humaine</h1>
            <div class="line"></div>
            
            <!-- "5.1" : "tract chaine humaine" -->
            <div class="card" style="text-align:left; width:100%; max-width:500px; overflow:hidden; position:relative; height:250px; background:rgba(0,0,0,0.4); border:1px solid var(--border); border-radius:8px;">
              <div class="card-title" style="padding: 10px 15px; margin: 0; background: rgba(0,0,0,0.2); border-bottom: 1px solid var(--border); font-weight: bold; text-transform: uppercase; font-size: 13px;">Tract chaîne humaine</div>
              <div id="tract-scroller-container" style="position:relative; width:100%; height:190px; overflow:hidden;">
                <div id="tract-scrolling-text" style="position:absolute; width:calc(100% - 20px); font-size:18px; line-height:1.6; color:var(--text); text-align:center; transform:translateY(180px); padding: 10px;">
                  Lorem ipsum dolor sit amet, consectetur adipiscing elit. <br><br>
                  Sed non risus. <br><br>
                  Suspendisse lectus tortor, dignissim sit amet, tempor ac, condimentum ac, nisi. <br><br>
                  Fin du tract.
                </div>
              </div>
            </div>

            <!-- "5.2" : "accord de principe" -->
            <div id="accord-card" class="card" style="text-align:left; width:100%; max-width:500px; margin-top:15px; display:none;">
              <div class="card-title">Accord de principe</div>
              <label style="display:flex; align-items:center; gap:10px; cursor:pointer; user-select:none; font-size:13px;">
                <input type="checkbox" id="contrast-checkbox" onchange="toggleContrastMode(this.checked)" style="width:20px; height:20px; cursor:pointer; accent-color:var(--primary);">
                <span>D'accord</span>
              </label>
            </div>
        </div>
    </div>

</div>

<div class="popover-reactions" id="popover-reactions">
  <button class="reaction-emoji-btn" onclick="vote('❕️')">❕️</button>
  <button class="reaction-emoji-btn" onclick="vote('❔️')">❔️</button>
  <button class="reaction-emoji-btn" onclick="vote('❌️')">❌️</button>
  <button class="reaction-emoji-btn" onclick="vote('⏸️')">⏸️</button>
  <button class="reaction-emoji-btn" onclick="vote('✔️')">✔️</button>
</div>

<!-- Paragraphe 6.3 : Code JavaScript (Logique client, AJAX, évènements et glisser-déposer) -->
<script>
    window.onerror = function(message, source, lineno, colno, error) {
      alert("⚠️ Erreur JavaScript détectée !\n\nMessage : " + message + "\nFichier : " + source + "\nLigne : " + lineno + "\nColonne : " + colno);
      return false;
    };

    let myFavorites = [];
    let myBanned = [];
    const cvsStockes = {};
    const docsStockes = {};
    let configSelectedMode = 0;
    let currentDuration = 8;
    let currentZoomDuration = 32;
    let currentMarqueeItem = null;
    let isSalonHost = false;

    let avatarState = {
      head_color: 'transparent',
      torso_color: '#e5e7eb',
      torso_pattern: '',
      legs_color: '#e5e7eb',
      legs_pattern: '',
      feet_color: '#e5e7eb'
    };

    const selectAvatarColor = (part, color) => {
      avatarState[`${part}_color`] = color;
      applyAvatarState();
    };

    const selectAvatarPattern = (part, pattern) => {
      avatarState[`${part}_pattern`] = pattern;
      applyAvatarState();
    };

    const applyAvatarState = () => {
      // Head
      let headEl = document.getElementById('part-head');
      if (headEl) {
        if (avatarState.head_color === 'transparent') {
          headEl.style.opacity = '0.3';
          headEl.style.backgroundColor = 'transparent';
        } else {
          headEl.style.opacity = '1';
          headEl.style.backgroundColor = avatarState.head_color;
        }
      }
      
      // Torso
      let torsoEl = document.getElementById('part-torso');
      if (torsoEl) {
        if (avatarState.torso_color === 'transparent') {
          torsoEl.style.opacity = '0.3';
          torsoEl.style.backgroundColor = 'transparent';
        } else {
          torsoEl.style.opacity = '1';
          torsoEl.style.backgroundColor = avatarState.torso_color;
        }
      }
      let torsoPatternEl = document.getElementById('pattern-torso');
      if (torsoPatternEl) {
        torsoPatternEl.className = 'pattern-overlay';
        if (avatarState.torso_pattern) torsoPatternEl.classList.add(avatarState.torso_pattern);
      }

      // Legs
      let legsEl = document.getElementById('part-legs');
      if (legsEl) {
        if (avatarState.legs_color === 'transparent') {
          legsEl.style.opacity = '0.3';
          legsEl.style.backgroundColor = 'transparent';
        } else {
          legsEl.style.opacity = '1';
          legsEl.style.backgroundColor = avatarState.legs_color;
        }
      }
      let legsPatternEl = document.getElementById('pattern-legs');
      if (legsPatternEl) {
        legsPatternEl.className = 'pattern-overlay';
        if (avatarState.legs_pattern) legsPatternEl.classList.add(avatarState.legs_pattern);
      }

      // Feet
      let footLEl = document.getElementById('part-foot-l');
      let footREl = document.getElementById('part-foot-r');
      if (footLEl && footREl) {
        if (avatarState.feet_color === 'transparent') {
          footLEl.style.opacity = '0.3';
          footLEl.style.backgroundColor = 'transparent';
          footREl.style.opacity = '0.3';
          footREl.style.backgroundColor = 'transparent';
        } else {
          footLEl.style.opacity = '1';
          footLEl.style.backgroundColor = avatarState.feet_color;
          footREl.style.opacity = '1';
          footREl.style.backgroundColor = avatarState.feet_color;
        }
      }

      // Active border swatches styling
      document.querySelectorAll('.color-swatch-btn').forEach(btn => btn.classList.remove('active'));
      document.querySelectorAll('.pattern-select-btn').forEach(btn => btn.classList.remove('active'));
    };

    const resetAvatar = () => {
      avatarState = {
        head_color: 'transparent',
        torso_color: 'transparent',
        torso_pattern: '',
        legs_color: 'transparent',
        legs_pattern: '',
        feet_color: 'transparent'
      };
      applyAvatarState();
    };

    const showModeSelection = () => {
      fermerOverlays();
      let modeScreen = document.getElementById('mode-selection-screen');
      if (modeScreen) {
        modeScreen.style.removeProperty('display');
        modeScreen.classList.remove('hidden');
      }
    };

    const toggleContrastMode = (checked) => {
      let fd = new URLSearchParams();
      fd.append('contrast', checked ? '1' : '0');
      fetch('/api/my_contrast', { method: 'POST', body: fd })
        .then(() => {
          fetch('/api/trigger_broadcast', { method: 'POST' });
        });
    };



    const confirmSalonUsername = () => {
      let username = document.getElementById('salon-modal-username-input').value.trim();
      if (username === "") {
        alert("Veuillez saisir un pseudonyme pour entrer dans le salon de discussion.");
        return;
      }
      sessionStorage.setItem('civvi_salon_username', username);
      let modal = document.getElementById('salon-username-modal');
      if (modal) {
        modal.style.setProperty('display', 'none', 'important');
        modal.classList.remove('actif');
      }
      document.getElementById('header-user').innerText = username;
    };




    const loadSdFiles = () => {
      let container = document.getElementById('sd-files-container');
      if (!container) return;
      fetch('/api/sd/list')
        .then(r => { if (!r.ok) throw new Error(); return r.json(); })
        .then(data => {
          container.innerHTML = "";
          if (data.length === 0) {
            container.innerHTML = '<div style="color:var(--text-muted); font-style:italic; text-align:center;">Aucun fichier partagé.</div>';
            return;
          }
          data.forEach(f => {
            let item = document.createElement('div');
            item.style = "display:flex; justify-content:space-between; padding:4px; border-bottom:1px solid rgba(255,255,255,0.1); align-items:center;";
            item.innerHTML = `
              <span style="overflow:hidden; text-overflow:ellipsis; white-space:nowrap; max-width:55%;" title="${f.name}">${f.name}</span>
              <span>
                <button class="btn btn-sec" style="padding:2px 6px; font-size:10px; margin-right:4px;" onclick="viewSdFile('${f.name}')">👁️</button>
                <a href="/api/sd/get?file=${encodeURIComponent(f.name)}" class="btn btn-sec" style="padding:2px 6px; font-size:10px; margin-right:4px;" download>📥</a>
                <button class="btn btn-danger" style="padding:2px 6px; font-size:10px;" onclick="deleteSdFile('${f.name}')">🗑️</button>
              </span>
            `;
            container.appendChild(item);
          });
        })
        .catch(err => {
          container.innerHTML = '<div style="color:#ef4444; font-style:italic; text-align:center;">Carte SD absente ou dossier partage manquant.</div>';
        });
    };

    const deleteSdFile = (name) => {
      if (confirm(`Supprimer "${name}" ?`)) {
        let fd = new URLSearchParams(); fd.append('file', name);
        fetch('/api/sd/delete', { method: 'POST', body: fd }).then(() => loadSdFiles());
      }
    };

    const uploadSdFile = () => {
      let input = document.getElementById('sd-file-input');
      if (input.files.length === 0) return alert("Sélectionnez un fichier.");
      let fd = new FormData(); fd.append("file", input.files[0]);
      let xhr = new XMLHttpRequest();
      xhr.open("POST", "/api/sd/upload", true);
      xhr.onload = () => {
        input.value = "";
        let lbl = document.getElementById('sd-selected-filename');
        if(lbl) { lbl.style.display = 'none'; lbl.innerText = ''; }
        alert("Fichier envoyé !");
      };
      xhr.send(fd);
    };

    const switchBackToP2P = () => {
      if (confirm("Voulez-vous repasser l'ESP en mode classique (P2P) et redémarrer ?")) {
        let fd = new URLSearchParams();
        fd.append('mode', '0');
        fetch('/api/set_mode', { method: 'POST', body: fd })
          .then(r => {
            if (r.ok) {
              localStorage.removeItem('civvi_is_owner');
              alert("Redémarrage en mode classique (P2P) en cours...");
              setTimeout(() => location.reload(), 2000);
            } else {
              alert("Erreur lors du changement de mode.");
            }
          });
      }
    };

    const escapeHtml = (text) => {
      return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;").replace(/'/g, "&#039;");
    };

    const viewSdFile = (filename) => {
      let contentEl = document.getElementById('doc-viewer-content');
      document.getElementById('doc-viewer-title').innerText = "Fichier SD - " + filename;
      contentEl.innerHTML = `<h3>Chargement de ${filename}...</h3><div class="spinner"></div>`;
      ouvrirOverlay('ov-doc-viewer');
      
      fetch('/api/sd/get?file=' + encodeURIComponent(filename))
        .then(res => {
          if (!res.ok) throw new Error();
          let ct = res.headers.get('content-type') || '';
          if (ct.includes('image') || filename.endsWith('.png') || filename.endsWith('.jpg') || filename.endsWith('.jpeg')) {
            contentEl.innerHTML = `<img src="/api/sd/get?file=${encodeURIComponent(filename)}" style="max-width:100%; max-height:70vh; border-radius:8px; display:block; margin:0 auto;" />`;
          } else {
            res.text().then(txt => {
              contentEl.innerHTML = `<pre style="white-space:pre-wrap; font-family:monospace; font-size:12px; margin:0; color:var(--text);">${escapeHtml(txt)}</pre>`;
            });
          }
        })
        .catch(() => {
          contentEl.innerText = "Impossible d'afficher ce fichier.";
        });
    };

    const insertMarkdown = (action) => {
      let textarea = document.getElementById('cv-markdown-editor');
      let start = textarea.selectionStart;
      let end = textarea.selectionEnd;
      let text = textarea.value;
      let selected = text.substring(start, end);
      
      let replacement = "";
      if (action === 'bold') {
        replacement = `**${selected || 'texte'}**`;
      } else if (action === 'italic') {
        replacement = `*${selected || 'texte'}*`;
      } else if (action === 'underline') {
        replacement = `<u>${selected || 'texte'}</u>`;
      } else if (action === 'h1') {
        replacement = `\n# ${selected || 'Titre 1'}\n`;
      } else if (action === 'h2') {
        replacement = `\n## ${selected || 'Titre 2'}\n`;
      } else if (action === 'list') {
        replacement = `\n- ${selected || 'élément'}\n`;
      } else if (action === 'quote') {
        replacement = `\n> ${selected || 'citation'}\n`;
      } else if (action === 'link') {
        replacement = `[${selected || 'texte du lien'}](https://example.com)`;
      }
      
      textarea.value = text.substring(0, start) + replacement + text.substring(end);
      textarea.focus();
      textarea.setSelectionRange(start + replacement.length, start + replacement.length);
    };

    const saveMarkdownCV = () => {
      let text = document.getElementById('cv-markdown-editor').value;
      fetch('/notes', {
        method: 'POST',
        body: text
      }).then(r => {
        if (r.ok) alert("Document enregistré avec succès !");
        else alert("Erreur d'enregistrement.");
      });
    };

    const viewPeerDoc = (mac, username) => {
      let viewer = document.getElementById('ov-doc-viewer');
      let content = document.getElementById('doc-viewer-content');
      document.getElementById('doc-viewer-title').innerText = "Centres d'intérêt - " + username;
      
      content.innerHTML = `<h3>Chargement du document de ${username}...</h3><div class="spinner"></div>`;
      viewer.classList.add('actif');
      
      let attempts = 0;
      let fd = new URLSearchParams();
      fd.append('mac', mac);
      
      fetch('/api/request_doc', { method: 'POST', body: fd })
        .then(() => {
          let interval = setInterval(() => {
            attempts++;
            fetch('/api/get_doc?mac=' + encodeURIComponent(mac))
              .then(r => {
                if (r.status === 200) {
                  clearInterval(interval);
                  return r.text();
                }
                if (attempts > 12) {
                  clearInterval(interval);
                  throw new Error("Timeout");
                }
                return null;
              })
              .then(text => {
                if (text !== null) {
                  docsStockes[mac] = text;
                  content.innerHTML = parseMarkdown(text);
                }
              })
              .catch(err => {
                content.innerHTML = `<h3>Impossible de charger le document de ${username}.</h3><p style="color:var(--text-muted); font-size:12px; text-align:center;">Le module est hors de portée ou n'a pas répondu.</p>`;
              });
          }, 1000);
        });
    };

    const toggleFavoritePeer = (mac, name) => {
      let macLower = mac.toLowerCase();
      if (name) {
        let favNames = {};
        try {
          favNames = JSON.parse(localStorage.getItem('civvi_fav_names') || '{}');
        } catch(e) {}
        favNames[macLower] = name;
        localStorage.setItem('civvi_fav_names', JSON.stringify(favNames));
      }
      let fd = new URLSearchParams();
      fd.append('mac', mac);
      fetch('/api/favorites/toggle', { method: 'POST', body: fd })
        .then(() => {
          fetch('/notes')
        .then(r => r.text())
        .then(text => {
          document.getElementById('cv-markdown-editor').value = text;
        });

      fetch('/api/config')
            .then(r => r.json())
            .then(data => {
              myFavorites = data.favorites || [];
              let viewer = document.getElementById('ov-profil-viewer');
              if (viewer && viewer.classList.contains('actif')) {
                let text = cvsStockes[mac] || "{}";
                document.getElementById('cv-viewer-content').innerHTML = renderStructuredProfile(text, name, mac);
              }
              update();
              loadFavoritesAndBannedUI();
            });
        });
    };

    const toggleBannedPeer = (mac, name) => {
      let fd = new URLSearchParams();
      fd.append('mac', mac);
      fetch('/api/banned/toggle', { method: 'POST', body: fd })
        .then(() => {
          fetch('/notes')
        .then(r => r.text())
        .then(text => {
          document.getElementById('cv-markdown-editor').value = text;
        });

      fetch('/api/config')
            .then(r => r.json())
            .then(data => {
              myBanned = data.banned || [];
              let viewer = document.getElementById('ov-cv-viewer');
              if (viewer) viewer.classList.remove('actif');
              update();
              loadFavoritesAndBannedUI();
            });
        });
    };

    const formatBytes = (bytes) => {
      if (bytes === 0) return '0 Octet';
      const k = 1024;
      const sizes = ['Octets', 'Ko', 'Mo', 'Go'];
      const i = Math.floor(Math.log(bytes) / Math.log(k));
      return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
    };

    const loadFavoritesAndBannedUI = () => {
      let favList = document.getElementById('favorites-list-container');
      
      if (favList) {
        favList.innerHTML = "";
        if (myFavorites.length === 0) {
          favList.innerHTML = '<div style="color:var(--text-muted); font-style:italic;">Aucun favori pour le moment.</div>';
        } else {
          let favNames = {};
          try {
            favNames = JSON.parse(localStorage.getItem('civvi_fav_names') || '{}');
          } catch(e) {}
          myFavorites.forEach(mac => {
            let macLower = mac.toLowerCase();
            let name = favNames[macLower] || mac;
            let div = document.createElement('div');
            div.style = "display:flex; justify-content:space-between; align-items:center; background:rgba(255,255,255,0.02); padding:4px 8px; border-radius:4px;";
            div.innerHTML = `
              <span>🌟 <strong style="color:var(--primary);">${name}</strong> <span style="font-size:9px; color:var(--text-muted);">(${mac})</span> 🌟</span>
              <button class="btn btn-danger" style="padding:2px 6px; font-size:9px;" onclick="toggleFavoritePeer('${mac}', '')">Retirer</button>
            `;
            favList.appendChild(div);
          });
        }
      }
    };

    const saveStructuredProfile = () => {
      let profile = {
        ...avatarState
      };

      fetch('/api/profile', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(profile)
      }).then(() => {
        alert("Profil enregistré !");
      });
    };

    const loadStructuredProfile = () => {
      fetch('/api/profile')
        .then(r => r.json())
        .then(data => {
          avatarState.head_color = data.head_color || 'transparent';
          avatarState.torso_color = data.torso_color || 'transparent';
          avatarState.torso_pattern = data.torso_pattern || '';
          avatarState.legs_color = data.legs_color || 'transparent';
          avatarState.legs_pattern = data.legs_pattern || '';
          avatarState.feet_color = data.feet_color || 'transparent';
          
          applyAvatarState();
        });
    };

    let activeTab = 'souche';
    let currentData = [];

    let seenGraffitis = JSON.parse(localStorage.getItem('civvi_seen_graffitis') || '[]');
    
    let sortMode = localStorage.getItem('civvi_sort') || 'fav_first';
    if (sortMode === 'rssi') sortMode = 'rssi_desc';
    if (sortMode === 'alpha') sortMode = 'alpha_asc';

    let isFrozen = localStorage.getItem('civvi_freeze') === 'true';
    let textScrollSpeed = parseFloat(localStorage.getItem('civvi_speed') || '1.0');
    let currentVoteCible = null;

    let firstUpdateDone = false;
    // Marquee news queue variables
    let marqueeQueue = [];
    let marqueeActive = false;
    let shownMessages = new Set();
    let expiredMacs = new Set(); // set of expired/offline MACs
    let espMode = 0; // 0 = Relais, 1 = Salon

    document.getElementById('sort-select').value = sortMode;
    document.getElementById('speed-slider').value = textScrollSpeed;
    document.getElementById('speed-value').innerText = textScrollSpeed.toFixed(1);


    // Dynamic control bar height update for absolute elements (like c11 and c12)
    const updateControlBarHeight = () => {
      const cb = document.querySelector('.control-bar');
      if (cb) {
        document.documentElement.style.setProperty('--control-bar-height', `${cb.offsetHeight}px`);
      }
    };
    window.addEventListener('resize', updateControlBarHeight);
    window.addEventListener('DOMContentLoaded', updateControlBarHeight);
    if (window.ResizeObserver) {
      const cb = document.querySelector('.control-bar');
      if (cb) {
        new ResizeObserver(updateControlBarHeight).observe(cb);
      }
    }

    // Vérifie si la pensée est trop ancienne pour figurer dans le bandeau d'actualités (limite 10 minutes)
    const isMessageTooOld = (text, maxAgeMinutes = 10) => {
      if (!text) return false;
      let match = text.match(/^\[(\d{2})\/(\d{2})\s(\d{2}):(\d{2})\]/);
      if (!match) return false;
      
      let day = parseInt(match[1], 10);
      let month = parseInt(match[2], 10) - 1; // 0-indexé
      let hour = parseInt(match[3], 10);
      let minute = parseInt(match[4], 10);
      
      let now = new Date();
      let msgDate = new Date(now.getFullYear(), month, day, hour, minute);
      
      if (msgDate > now && month === 11 && now.getMonth() === 0) {
        msgDate.setFullYear(now.getFullYear() - 1);
      }
      
      let diffMs = now - msgDate;
      let diffMinutes = diffMs / 1000 / 60;
      
      return (diffMinutes > maxAgeMinutes || diffMinutes < -5);
    };

    window.switchToSalonMode = function() {
      if (confirm("Voulez-vous activer le mode Salon de discussion et redémarrer ?")) {
        let ssid = document.getElementById('config-ssid').value.trim();
        let pwd = document.getElementById('config-pwd').value;
        let username = "L'hébergeur";
        let rebootMsg = document.getElementById('config-reboot-msg').value.trim();
        let rediffuseLast = document.getElementById('config-rediffuse-last').checked ? "1" : "0";

        if (ssid === "") return alert("Le SSID ne peut pas être vide !");
        
        let fd = new URLSearchParams();
        fd.append('ssid', ssid);
        fd.append('pwd', pwd);
        fd.append('username', username);
        fd.append('rebootMsg', rebootMsg);
        fd.append('mode', '1');
        fd.append('rediffuseLast', rediffuseLast);
        
        localStorage.setItem('civvi_is_owner', 'true');
        localStorage.setItem('civvi_salon_username', "L'hébergeur");
        sessionStorage.setItem('civvi_salon_username', "L'hébergeur");

        fetch('/api/config', { method: 'POST', body: fd })
          .then(() => {
            alert("Mode Salon activé ! Redémarrage en cours... Reconnectez-vous au réseau Wi-Fi : " + ssid + "-salon");
            setTimeout(() => {
              location.reload();
            }, 2000);
          });
      }
    };;



    let scanFinished = false;
    setTimeout(() => {
      scanFinished = true;
      updateP2PScanStatus();
    }, 4000);

    function updateP2PScanStatus() {
      let statusEl = document.getElementById('p2p-scan-status');
      if (!statusEl) return;
      if (!scanFinished) {
        statusEl.innerText = "Recherche de modules voisins en cours...";
      } else {
        let count = currentData ? (currentData.length - 1) : 0;
        if (count < 0) count = 0;
        statusEl.innerHTML = `<span class="zoom-animate" style="color: var(--primary);">${count}</span> module(s) voisin(s) détecté(s)`;
      }
    }

    function drawStaticQRCode(containerId) {
      const matrix = [
        [1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1],
        [1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1],
        [1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1],
        [1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1],
        [1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1],
        [1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1],
        [1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1],
        [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0],
        [1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 1],
        [1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0],
        [0, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 1],
        [1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1],
        [0, 0, 1, 1, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1],
        [1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0],
        [1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1],
        [1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1],
        [1, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0],
        [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0],
        [1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 1],
        [1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0],
        [1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1],
        [1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1],
        [1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1],
        [1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1],
        [1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1]
      ];
      let size = 200;
      let box = size / 25;
      let svg = `<svg width="${size}" height="${size}" viewBox="0 0 ${size} ${size}" style="background:white; padding:10px; border-radius:8px; box-shadow: 0 4px 15px rgba(0,0,0,0.15);">`;
      for (let r = 0; r < 25; r++) {
        for (let c = 0; c < 25; c++) {
          if (matrix[r][c] === 1) {
            svg += `<rect x="${c * box}" y="${r * box}" width="${box + 0.5}" height="${box + 0.5}" fill="black" />`;
          }
        }
      }
      svg += "</svg>";
      let el = document.getElementById(containerId);
      if (el) el.innerHTML = svg;
    }





     window.startBootSequence = function() {
      let splash = document.getElementById('splash-screen');
      if (splash) {
        splash.style.setProperty('display', 'none', 'important');
        splash.classList.add('hidden');
      }

      if (espMode === 1) {
        // Mode Salon
        document.body.classList.add('salon-active');
        if (isSalonHost) {
          sessionStorage.setItem('civvi_salon_username', "L'hébergeur");
          document.getElementById('header-user').innerText = "L'hébergeur";
          let btn = document.getElementById('btn-admin-p2p');
          if (btn) {
            btn.innerText = "Fermer le Salon 🚪";
            btn.style.display = 'block';
          }
        } else {
          document.body.classList.add('salon-mode');
          let savedUsername = sessionStorage.getItem('civvi_salon_username');
          if (savedUsername === "L'hébergeur") {
            sessionStorage.removeItem('civvi_salon_username');
            savedUsername = null;
          }
          if (!savedUsername) {
            let modal = document.getElementById('salon-username-modal');
            if (modal) {
              modal.style.setProperty('display', 'flex', 'important');
              modal.classList.add('actif');
            }
          } else {
            document.getElementById('header-user').innerText = savedUsername;
          }
        }
      } else {
        // Mode P2P (Relais)
        document.body.classList.remove('salon-active');
        document.body.classList.remove('salon-mode');
      }
    };




    window.onload = () => {
      const maintenant = new Date();
      const epochSecondes = Math.floor(maintenant.getTime() / 1000);
      const timezoneOffset = maintenant.getTimezoneOffset() * -60;
      fetch('/api/sync-time', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'time=' + epochSecondes + '&offset=' + timezoneOffset
      });

      fetch('/notes')
        .then(r => r.text())
        .then(text => {
          document.getElementById('cv-markdown-editor').value = text;
        });

      fetch('/api/config')
        .then(r => r.json())
        .then(data => {
          document.getElementById('config-ssid').value = data.ssid || "";
          document.getElementById('config-pwd').value = data.pwd || "";
          document.getElementById('config-username').value = data.username || "";
          document.getElementById('config-reboot-msg').value = data.rebootMsg || "";
          configSelectedMode = data.mode !== undefined ? parseInt(data.mode) : 0;
          if (document.getElementById('config-rediffuse-last')) {
            document.getElementById('config-rediffuse-last').checked = data.rediffuseLast === 1;
          }

          espMode = data.mode !== undefined ? parseInt(data.mode) : 0;
          isSalonHost = data.isSalonHost === true;

          // Set parameters based on URL query
          let urlParams = new URLSearchParams(window.location.search);
          if (urlParams.get('tab') === 'accueil') {
            ouvrirOverlay('ov-lorem');
          }



          if (data.username) monNomUtilisateur = data.username;
          myFavorites = data.favorites || [];
          myBanned = data.banned || [];
          loadFavoritesAndBannedUI();
          
          let fsText = document.getElementById('fs-space-info');
          if (fsText && data.fsTotal) {
            let freeSpace = data.fsTotal - data.fsUsed;
            let percent = ((data.fsUsed / data.fsTotal) * 100).toFixed(1);
            fsText.innerHTML = `Espace : ${formatBytes(data.fsUsed)} utilisé / ${formatBytes(freeSpace)} libre (Total : ${formatBytes(data.fsTotal)} &bull; ${percent}%)`;
          }

          let contrastChk = document.getElementById('contrast-checkbox');
          if (contrastChk) contrastChk.checked = (data.contrast === 1);
        });

      const templateMarkdown = `# 🌿 Prénom Nom\n## Identité & Rôle\n> "Créer des ponts entre le cyber-espace et la nature."\n\n---\n\n### 🛠️ Compétences\n- **Réseau décentralisé** : Protocoles ESP-NOW et Wi-Fi ad-hoc\n- **Matériel** : Microcontrôleurs ESP32, capteurs environnementaux\n- **Conception** : Interfaces web durables et minimalistes\n\n### 🌍 Projets & Contributions\n- **Projet Civvi** : Déploiement de relais cyber-naturels autonomes.\n- [En savoir plus sur Civvi](https://github.com/relais-civvi)\n\n---\n\n### 📬 Me contacter\n- *Mail* : contact@civvi.local\n- *Relais local* : Canal 1`;

      if (document.getElementById('cv-editor-text')) {
        fetch('/notes')
          .then(r => r.text())
          .then(text => {
            document.getElementById('cv-editor-text').value = text && text.trim() ? text : templateMarkdown;
          });
      }

      update();
      setInterval(loadNotifications, 5000);
      setInterval(update, 20000);



      // Initialize badge polling
      setInterval(checkNewFeuilleMessages, 5000);
      checkNewFeuilleMessages();

      // Initialize Avatar and Profile loader
      applyAvatarState();
      loadStructuredProfile();
    };

    window.entrerInterface = () => {
      let screen = document.getElementById('screen-demarrage');
      if (screen) {
        screen.style.opacity = '0';
        screen.style.visibility = 'hidden';
        setTimeout(() => {
          screen.style.display = 'none';
        }, 500);
      }
      window.startBootSequence();
    };

    const ouvrirOverlay = (idOverlay) => {
      let openingActu = (idOverlay === 'ov-actu');
      fermerOverlays();
      document.getElementById(idOverlay).classList.add('actif');
      document.getElementById('btn-configuration').style.opacity = '0';
      document.getElementById('btn-configuration').style.pointerEvents = 'none';
      document.getElementById('btn-profil').style.opacity = '0';
      document.getElementById('btn-profil').style.pointerEvents = 'none';
      document.getElementById('btn-messagerie').style.opacity = '0';
      document.getElementById('btn-messagerie').style.pointerEvents = 'none';
      let btnAcc = document.getElementById('btn-accueil');
      if (btnAcc) btnAcc.setAttribute('title', "retour à l'interface principale");
      activeTab = idOverlay;
      if (idOverlay === 'ov-lorem') {
        let scroller = document.getElementById('tract-scrolling-text');
        let accordCard = document.getElementById('accord-card');
        if (scroller && accordCard) {
          accordCard.style.display = 'none';
          scroller.classList.remove('tract-scrolling');
          scroller.offsetHeight;
          scroller.classList.add('tract-scrolling');
          scroller.onanimationend = () => {
            accordCard.style.display = 'block';
          };
        }
      }
      if (idOverlay === 'ov-messagerie') {
        loadReceivedGraffitis();
      } else if (idOverlay === 'ov-profil') {
        loadBroadcastHistory();
      }
      
      if (openingActu) {
        setupMarqueeEndListener();
        let actuZoom = document.getElementById('ov-actu-marquee');
        if (actuZoom) {
          actuZoom.style.animation = "none";
          actuZoom.offsetHeight;
          actuZoom.style.position = "absolute";
          actuZoom.style.animation = `defilementD-G ${currentZoomDuration}s linear forwards`;
        }
      }
    };

    const fermerOverlays = (userClicked = false) => {


      let anyActive = false;
      let closingActu = false;
      document.querySelectorAll('.overlay').forEach(ov => {
        if (ov.classList.contains('actif')) {
          anyActive = true;
          if (ov.id === 'ov-actu') {
            closingActu = true;
          }
          ov.classList.remove('actif');
        }
      });
      document.getElementById('btn-configuration').style.opacity = '1';
      document.getElementById('btn-configuration').style.pointerEvents = 'auto';
      document.getElementById('btn-profil').style.opacity = '1';
      document.getElementById('btn-profil').style.pointerEvents = 'auto';
      document.getElementById('btn-messagerie').style.opacity = '1';
      document.getElementById('btn-messagerie').style.pointerEvents = 'auto';
      activeTab = 'souche';

      let btnAcc = document.getElementById('btn-accueil');
      if (btnAcc) btnAcc.setAttribute('title', "chaine humaine");

      if (userClicked && !anyActive) {
        ouvrirOverlay('ov-lorem');
      }

      if (closingActu) {
        setupMarqueeEndListener();
        let actuMarquee = document.getElementById('marquee-actu-text');
        if (actuMarquee) {
          actuMarquee.style.animation = "none";
          actuMarquee.offsetHeight;
          actuMarquee.style.animation = `defilementD-G ${currentDuration}s linear forwards`;
        }
      }
    };



    document.getElementById('speed-slider').oninput = (e) => {
      textScrollSpeed = parseFloat(e.target.value);
      document.getElementById('speed-value').innerText = textScrollSpeed.toFixed(1);
      localStorage.setItem('civvi_speed', textScrollSpeed);
      document.querySelectorAll('.scrolling-text').forEach(anim => {
        let textLen = anim.dataset.raw.length + 15;
        let duree = Math.max(8, textLen * 0.12) / textScrollSpeed;
        anim.style.animationDuration = `${duree}s`;
      });
    };

    document.getElementById('msg').oninput = (e) => {
      document.getElementById('char-counter').innerText = `${e.target.value.length} / 3500`;
    };
    document.getElementById('msg').onkeydown = (e) => {
      if (e.key === 'Enter') {
        e.preventDefault();
        sendThought();
        document.getElementById('msg').blur();
      }
    };

    const couleurDepuisMac = (mac) => {
      let hash = 0;
      for (let i = 0; i < mac.length; i++) { hash = mac.charCodeAt(i) + ((hash << 5) - hash); }
      let hue = Math.abs(hash) % 360;
      return `hsl(${hue}, 70%, 75%)`;
    };

    const sendThought = () => {
      let text = document.getElementById('msg').value.trim();
      if (text === "") return;
      
      let url = '/send';
      if (espMode === 1) {
        let username = sessionStorage.getItem('civvi_salon_username') || "Visiteur";
        url += '?user=' + encodeURIComponent(username);
      }
      
      fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'text/plain; charset=utf-8' },
        body: text
      }).then(() => {
        document.getElementById('msg').value = "";
        localStorage.removeItem('civvi_my_emoticon');
        localStorage.removeItem('civvi_my_emoticon_count');
        document.getElementById('char-counter').innerText = "0 / 3500";
        fermerOverlays();
        update();
        
        // Reset mobile viewport zoom and focus
        document.querySelectorAll('input, textarea').forEach(el => el.blur());
        window.scrollTo(0, 0);
      });
    };

    const toggleFreeze = (val) => {
      isFrozen = val;
      localStorage.setItem('civvi_freeze', isFrozen);
    };

    const changeSortMode = (val) => {
      sortMode = val;
      localStorage.setItem('civvi_sort', sortMode);
      update();
    };

    let isScratching = false;
    let currentElem = null;
    let startX = 0;
    let startY = 0;
    let lastX = 0;
    let lastTime = 0;
    let startOffset = 0;
    let velocity = 0;
    let inertiaId = null;
    let dragDirectionDecided = false;
    let isVerticalDrag = false;

    const getCurrentOffset = (elem) => {
      let t = window.getComputedStyle(elem).transform;
      if (t && t !== 'none') { let m = t.match(/matrix\(([^)]+)\)/); if (m) return parseFloat(m[1].split(',')[4] || 0); }
      return parseFloat(elem.dataset.offset || 0) || 0;
    };

    const startScratch = (e, elem) => {
      if (inertiaId) { cancelAnimationFrame(inertiaId); inertiaId = null; }
      isScratching = true;
      currentElem = elem;
      elem.style.animation = "none";
      const point = e.touches ? e.touches[0] : e;
      startX = point.clientX;
      startY = point.clientY;
      lastX = point.clientX;
      lastTime = performance.now();
      startOffset = getCurrentOffset(elem);
      elem.dataset.offset = startOffset;
      dragDirectionDecided = false;
      isVerticalDrag = false;
    };

    const moveScratch = (e) => {
      if (!isScratching || !currentElem) return;
      const point = e.touches ? e.touches[0] : e;
      let x = point.clientX;
      let y = point.clientY;

      if (!dragDirectionDecided) {
        let dx = Math.abs(x - startX);
        let dy = Math.abs(y - startY);
        if (dx > 6 || dy > 6) {
          dragDirectionDecided = true;
          if (dy > dx) {
            isVerticalDrag = true;
            isScratching = false;
            let tempElem = currentElem;
            currentElem = null;
            let textLen = tempElem.dataset.raw.length + 15;
            let duree = Math.max(8, textLen * 0.12) / textScrollSpeed;
            tempElem.style.animation = `scroll-left ${duree}s linear infinite`;
            return;
          }
        } else {
          return;
        }
      }

      if (isVerticalDrag) return;

      if (e.cancelable) e.preventDefault();

      let now = performance.now();
      let delta = x - startX;
      let newOffset = startOffset + delta;
      currentElem.dataset.offset = newOffset;
      currentElem.style.transform = `translateX(${newOffset}px)`;
      let dx = x - lastX;
      let dt = now - lastTime;
      if (dt > 0) velocity = dx / dt;
      lastX = x;
      lastTime = now;
    };

    const endScratch = () => {
      if (!isScratching || !currentElem) return;
      let elem = currentElem;
      let offset = parseFloat(elem.dataset.offset || 0);
      let v = velocity;
      isScratching = false;
      currentElem = null;
      const friction = 0.95;
      const step = () => {
        v *= friction;
        offset += v * 16;
        elem.dataset.offset = offset;
        elem.style.transform = `translateX(${offset}px)`;
        if (Math.abs(v) < 0.01) {
          elem.style.transform = "none";
          let textLen = elem.dataset.raw.length + 15;
          let duree = Math.max(8, textLen * 0.12) / textScrollSpeed;
          
          let w = elem.offsetWidth || 1;
          let fraction = (-offset) / w;
          fraction = ((fraction % 1) + 1) % 1;
          let timeOffset = fraction * duree;

          elem.style.animation = `scroll-left ${duree}s linear -${timeOffset.toFixed(3)}s infinite`;
          inertiaId = null;
          return;
        }
        inertiaId = requestAnimationFrame(step);
      };
      inertiaId = requestAnimationFrame(step);
    };

    document.addEventListener("mousemove", moveScratch);
    document.addEventListener("touchmove", moveScratch, {passive: false});
    document.addEventListener("mouseup", endScratch);
    document.addEventListener("touchend", endScratch);

    window.setupMarqueeEndListener = () => {
      let isFullScreen = document.getElementById('ov-actu').classList.contains('actif');
      let actuMarquee = document.getElementById('marquee-actu-text');
      let actuZoom = document.getElementById('ov-actu-marquee');
      
      actuMarquee.onanimationend = null;
      actuZoom.onanimationend = null;
      
      if (isFullScreen) {
        actuZoom.onanimationend = () => {
          marqueeActive = false;
          startMarquee();
        };
      } else {
        actuMarquee.onanimationend = () => {
          marqueeActive = false;
          startMarquee();
        };
      }
    };

    const startMarquee = () => {
      if (marqueeActive) return;
      
      let actuMarquee = document.getElementById('marquee-actu-text');
      let actuZoom = document.getElementById('ov-actu-marquee');
      
      if (marqueeQueue.length > 0) {
        marqueeActive = true;
        let currentItem = marqueeQueue.shift();
        currentMarqueeItem = currentItem;
        
        let datePart = "";
        let messagePart = currentItem.texte;
        let match = currentItem.texte.match(/^(\[\d{2}\/\d{2}\s\d{2}:\d{2}\]\s*)(.*)$/);
        if (match) {
          datePart = match[1];
          messagePart = match[2];
        }
        
        let username = currentItem.username || currentItem.nomReseau;
        let normalHtml = `<strong>${username}</strong> : ${messagePart} &nbsp;&nbsp;&nbsp; 🌿`;
        let giantHtml = `
          <span style="font-size: 7vh; vertical-align: middle; color: var(--secondary); font-family:-apple-system, BlinkMacSystemFont, sans-serif; font-weight: bold; margin-right: 30px;">${username} :</span>
          <span style="font-size: 82vh; vertical-align: middle; line-height: 1.1; color: var(--primary); font-family: Georgia, serif; font-style: italic; text-shadow: 0 0 35px rgba(217, 149, 43, 0.6);">${messagePart}</span>
          <span style="font-size: 15vh; vertical-align: middle; color: var(--secondary); margin-left: 20px;">🌿</span>
        `;
        
        actuMarquee.innerHTML = normalHtml;
        actuZoom.innerHTML = giantHtml;
        
        shownMessages.add(currentItem.key);
        
        let textLen = actuMarquee.innerText.length;
        currentDuration = Math.max(8, textLen * 0.15);
        currentZoomDuration = currentDuration * 4;
        
        actuMarquee.style.animation = "none";
        actuZoom.style.animation = "none";
        actuZoom.style.textAlign = "left";
        
        // Trigger reflow
        actuMarquee.offsetHeight;
        actuZoom.offsetHeight;
        
        actuMarquee.style.animation = `defilementD-G ${currentDuration}s linear forwards`;
        actuZoom.style.position = "absolute";
        actuZoom.style.animation = `defilementD-G ${currentZoomDuration}s linear forwards`;
        
        setupMarqueeEndListener();
      } else {
        currentMarqueeItem = null;
        actuMarquee.innerHTML = "En attente d'actualité...";
        actuZoom.innerHTML = `<span style="font-size: 10vh; color: var(--primary); font-family: Georgia, serif; font-style: italic; text-shadow: 0 0 25px rgba(217, 149, 43, 0.5);">En attente d'actualité...</span>`;
        actuMarquee.style.animation = "none";
        actuZoom.style.animation = "none";
        actuZoom.style.position = "static";
        actuZoom.style.textAlign = "center";
        actuMarquee.onanimationend = null;
        actuZoom.onanimationend = null;
      }
    };

    const update = () => {
      fetch('/messages')
        .then(r => { if (!r.ok) throw new Error(); return r.json(); })
        .then(data => {
          currentData = data;
          updateP2PScanStatus();
          let myNode = data[0];
          if (espMode === 1) {
            document.getElementById('header-user').innerText = localStorage.getItem('civvi_salon_username') || "Visiteur";
            document.getElementById('header-ssid').innerText = `Salon : ${myNode.nomReseau || "Inconnu"}`;
            document.getElementById('module-count').innerText = `${data.length - 1} Message(s)`;
            document.getElementById('my-thought-marquee').innerHTML = `Salon de discussion actif : <strong>${myNode.nomReseau || "Inconnu"}</strong>`;
          } else {
            document.getElementById('header-user').innerText = myNode.username || "Civvi";
            document.getElementById('header-ssid').innerText = `Réseau : ${myNode.nomReseau || "Inconnu"}`;
            document.getElementById('module-count').innerText = `${data.length - 1} Module(s)`;
            let myThoughtText = myNode.texte || "";
            let cleanMyThought = myThoughtText.replace(/^\[\d{2}\/\d{2}\s\d{2}:\d{2}\]\s*/, "");
            
            // Récupération et formatage des émoticones reçus sur notre message en cours (Item 2)
            let myVotesHtml = "";
            if (myNode.votes) {
              Object.entries(myNode.votes).forEach(([emoji, val]) => {
                if (val > 0) {
                  myVotesHtml += ` <span class="vote-badge" style="margin-left: 6px; font-style: normal; font-size: 11px; vertical-align: middle;">${emoji} ${val}</span>`;
                }
              });
            }
            
            if (cleanMyThought.trim() === "") {
              document.getElementById('my-thought-marquee').innerHTML = `Message diffusé : <em>(aucun)</em>`;
            } else {
              document.getElementById('my-thought-marquee').innerHTML = `Message diffusé : <strong>${myThoughtText}</strong>${myVotesHtml}`;
            }
          }

          if (!firstUpdateDone) {
            firstUpdateDone = true;
          }

          let othersData = data.slice(1);
          
          // Filter out banned MACs
          othersData = othersData.filter(m => !myBanned.includes(m.auteur.toLowerCase()));
          
          // Alert and connection notification for online favorites
          othersData.forEach(m => {
            let mac = m.auteur.toLowerCase();
            let name = (m.username && m.username.trim()) ? m.username : m.nomReseau;
            let age = m.age || 0;
            if (myFavorites.includes(mac) && age < 30000) {
              if (!window.notifiedFavorites) window.notifiedFavorites = new Set();
              if (!window.notifiedFavorites.has(mac)) {
                window.notifiedFavorites.add(mac);
                let area = document.getElementById('notifications-area');
                if (area) {
                  area.innerHTML += `
                    <div class="notification-banner" style="background: rgba(217, 149, 43, 0.15); border-color: var(--primary); border-left-color: var(--primary); padding:8px 12px; margin-bottom:8px; border-radius:6px; border-left-width:4px; font-size:12px;">
                      🌟 <strong>${name}</strong> est en ligne !
                    </div>`;
                }
              }
            }
          });
          
          // Manage marquee news queue (only for non-expired peers)
          othersData.forEach(m => {
            if (m.age < 30000) {
              expiredMacs.delete(m.auteur); // Reactive wakeup if signal returns!
            }
            let key = m.auteur + "_" + (m.texte || "");
            m.key = key;
            let alreadyInQueue = marqueeQueue.some(q => q.key === key);
            if (m.texte) {
              let cleanText = m.texte.replace(/^\[\d{2}\/\d{2}\s\d{2}:\d{2}\]\s*/, "");
              if (cleanText.trim() !== "" && !shownMessages.has(key) && !alreadyInQueue && m.age < 30000) {
                if (!isMessageTooOld(m.texte)) {
                  marqueeQueue.push(m);
                }
              }
            }
          });
          
          // Cleanup shown messages that disappeared
          let currentKeys = new Set(othersData.map(o => o.auteur + "_" + (o.texte || "")));
          shownMessages.forEach(k => {
            if (!currentKeys.has(k)) {
              shownMessages.delete(k);
            }
          });
          
          startMarquee();

          let container = document.getElementById('prompters');
          let existingIds = new Set(Array.from(container.children).map(c => c.id));
          let newIds = new Set();

          // We display only neighboring nodes (excluding our own)
          // And we filter out fully expired modules
          let activeOthers = othersData.filter(m => m.age <= 20000);

          // Sort activeOthers based on sortMode
          if (sortMode === 'fav_first') {
            activeOthers.sort((a, b) => {
              let favA = myFavorites.includes(a.auteur.toLowerCase()) ? 1 : 0;
              let favB = myFavorites.includes(b.auteur.toLowerCase()) ? 1 : 0;
              if (favA !== favB) return favB - favA; // Mode standard : favoris en premier (index plus faible)
              return b.rssi - a.rssi; // Puis tri par signal décroissant
            });
          } else if (sortMode === 'rssi_desc') {
            activeOthers.sort((a, b) => b.rssi - a.rssi);
          } else if (sortMode === 'rssi_asc') {
            activeOthers.sort((a, b) => a.rssi - b.rssi);
          } else if (sortMode === 'alpha_asc') {
            activeOthers.sort((a, b) => {
              let nameA = (a.username && a.username.trim()) ? a.username : a.nomReseau;
              let nameB = (b.username && b.username.trim()) ? b.username : b.nomReseau;
              return nameA.localeCompare(nameB, 'fr', { sensitivity: 'base' });
            });
          } else if (sortMode === 'alpha_desc') {
            activeOthers.sort((a, b) => {
              let nameA = (a.username && a.username.trim()) ? a.username : a.nomReseau;
              let nameB = (b.username && b.username.trim()) ? b.username : b.nomReseau;
              return nameB.localeCompare(nameA, 'fr', { sensitivity: 'base' });
            });
          }

          activeOthers.forEach((m, index) => {
            let id = "msg-" + m.auteur;
            newIds.add(id);
            let line = document.getElementById(id);
            let macLower = m.auteur.toLowerCase();
            let isFav = myFavorites.includes(macLower);
            let favStar = isFav ? "🌟 " : "";
            let nameDisplay = isFav ? `🌟 ${m.username || m.nomReseau} 🌟` : (m.username || m.nomReseau);
            
            let displayTexte = m.texte || "";
            let cleanText = displayTexte.replace(/^\[\d{2}\/\d{2}\s\d{2}:\d{2}\]\s*/, "");
            if (cleanText.trim() === "") {
              displayTexte = "(aucune pensée diffusée)";
            }
            let textFormatted = displayTexte + " &nbsp;&nbsp;&nbsp; 🌿 &nbsp;&nbsp;&nbsp; ";
            let color = couleurDepuisMac(m.username || m.auteur);

            let votesHtml = "";
            if (m.votes) {
              Object.entries(m.votes).forEach(([emoji, val]) => {
                if (val > 0) votesHtml += `<span class="vote-badge">${emoji} ${val}</span>`;
              });
            }

            if (line) {
              if (isFav) {
                line.style.boxShadow = "0 0 10px rgba(217, 149, 43, 0.3)";
                line.style.borderWidth = "2.5px";
                line.style.borderStyle = "solid";
              } else {
                line.style.boxShadow = "none";
                line.style.borderWidth = "1px";
              }
              let nameSpanEl = line.querySelector('.cartouche span');
              if (nameSpanEl) {
                nameSpanEl.innerText = nameDisplay;
              }
            }

            let neighborContrastClass = (m.contrast === 1) ? " neighbor-contrast" : "";
            if (!line) {
              let textLen = textFormatted.length + 15;
              let duree = Math.max(8, textLen * 0.12) / textScrollSpeed;
              
              line = document.createElement('div');
              line.className = "esp-line" + neighborContrastClass;
              line.id = id;
              line.style.borderColor = color;
              
              // "1.7.3" : nom clicable pour passer le voisin en favoris
              let cartouche = document.createElement('div');
              cartouche.className = "cartouche";
              cartouche.style.color = color;
              
              let nameSpan = document.createElement('span');
              nameSpan.innerText = nameDisplay;
              cartouche.style.cursor = "pointer";
              const toggleFav = (e) => {
                e.stopPropagation();
                e.preventDefault();
                toggleFavoritePeer(m.auteur, m.username || m.nomReseau);
              };
              cartouche.onclick = toggleFav;
              cartouche.ontouchstart = toggleFav;
              cartouche.appendChild(nameSpan);
              
              // "1.7.4" : ligne de message, avec possibilité de défilement manuel
              let riviere = document.createElement('div');
              riviere.className = "riviere";
              
              let anim = document.createElement('div');
              anim.className = "scrolling-text";
              anim.id = "text-" + id;
              anim.innerHTML = textFormatted;
              anim.dataset.raw = displayTexte;
              anim.dataset.offset = 0;
              anim.style.animationDuration = `${duree}s`;

              anim.addEventListener("mousedown", (e) => startScratch(e, anim));
              anim.addEventListener("touchstart", (e) => { startScratch(e, anim); });
              
              anim.addEventListener("animationiteration", () => {
                // Seamlessly swap pending text at the end of the scroll cycle
                if (anim.dataset.pendingText) {
                  anim.innerHTML = anim.dataset.pendingTextFormatted;
                  anim.dataset.raw = anim.dataset.pendingText;
                  let dur = parseFloat(anim.dataset.pendingDuree);
                  anim.style.animationDuration = `${dur}s`;
                  delete anim.dataset.pendingText;
                  delete anim.dataset.pendingTextFormatted;
                  delete anim.dataset.pendingDuree;
                }

                // Removed legacy scroll deletion check
              });

              riviere.appendChild(anim);

               // "1.7.1" : icone pour accéder au profil vestimentaire
              let cvBtn = document.createElement('button');
              cvBtn.className = "cv-case-btn";
              cvBtn.innerHTML = '<svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" style="display:inline-block; vertical-align:middle; color:var(--text-muted);"><circle cx="12" cy="5" r="3.5" /><rect x="8" y="10" width="8" height="5.5" rx="1" /><rect x="9.5" y="17" width="2" height="4.5" /><rect x="12.5" y="17" width="2" height="4.5" /><rect x="8.5" y="22.5" width="2" height="1" /><rect x="13.5" y="22.5" width="2" height="1" /></svg>';
              cvBtn.title = "Consulter le profil";
              cvBtn.onclick = () => viewPeerCV(m.auteur, m.username || m.nomReseau);
              cvBtn.style.marginRight = "4px";

              // "1.7.2" : icone pour accéder à la fiche de présentation
              let docBtn = document.createElement('button');
              docBtn.className = "cv-case-btn";
              docBtn.innerHTML = '<svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="display:inline-block; vertical-align:middle; color:var(--text-muted);"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"></path><polyline points="14 2 14 8 20 8"></polyline><line x1="16" y1="13" x2="8" y2="13"></line><line x1="16" y1="17" x2="8" y2="17"></line><polyline points="10 9 9 9 8 9"></polyline></svg>';
              docBtn.title = "Consulter les centres d'intérêt";
              docBtn.onclick = () => viewPeerDoc(m.auteur, m.username || m.nomReseau);
              docBtn.style.marginRight = "6px";

              // "1.7.5" : icone d'emoticon pour commenter un message
              let voteBtn = document.createElement('button');
              voteBtn.className = "vote-case-btn";
              voteBtn.innerText = "✔️";
              voteBtn.onclick = (e) => {
                showReactionsPicker(e, m.auteur);
              };

              // "1.7.6" : espace pour de décompte des d'emoticons reçus pour le message en cours de l'utilsateur
              let votesContainer = document.createElement('div');
              votesContainer.className = "votes-display";
              votesContainer.id = "votes-" + id;
              votesContainer.innerHTML = votesHtml;

              // "1.7.7" : icone lettre pour laisser un message
              let ghostContainer = document.createElement('button');
              ghostContainer.className = "ghost-btn-container";
              ghostContainer.title = "Laisser un graffiti postal";
              ghostContainer.onclick = (e) => {
                e.stopPropagation();
                let msg = prompt("Laissez un graffiti postal sur le mur de : " + (m.username || m.nomReseau) + "\n(150 caractères max)");
                if (msg && msg.trim() !== "") {
                  let fd = new URLSearchParams();
                  fd.append('cible', m.auteur);
                  fd.append('message', msg.substring(0, 150));
                  fetch('/api/remote_graffiti', { method: 'POST', body: fd })
                    .then(res => res.text())
                    .then(text => {
                      if (text.startsWith("PENDING:")) {
                        alert(text.substring(8));
                      } else if (text.startsWith("ENVOYE:")) {
                        alert(text.substring(7));
                      } else {
                        alert(text);
                      }
                    });
                }
              };
              let ghostBtn = document.createElement('div');
              ghostBtn.className = "ghost-btn";
              ghostBtn.innerHTML = "✉️";
              ghostContainer.appendChild(ghostBtn);

              line.appendChild(cvBtn);
              line.appendChild(docBtn);
              line.appendChild(cartouche);
              line.appendChild(riviere);
              line.appendChild(voteBtn);
              line.appendChild(votesContainer);
              line.appendChild(ghostContainer);
              
              container.appendChild(line);
            } else {
              // Node exists. Properties updated at start of iteration.
              
              if (m.contrast === 1) {
                line.classList.add('neighbor-contrast');
              } else {
                line.classList.remove('neighbor-contrast');
              }

              let anim = document.getElementById("text-" + id);
              if (anim.dataset.raw !== displayTexte && currentElem !== anim) {
                let textLen = textFormatted.length + 15;
                let duree = Math.max(8, textLen * 0.12) / textScrollSpeed;
                // Stage the update for the end of the current iteration loop
                anim.dataset.pendingText = displayTexte;
                anim.dataset.pendingTextFormatted = textFormatted;
                anim.dataset.pendingDuree = duree;
              }
              document.getElementById("votes-" + id).innerHTML = votesHtml;
            }
            
            // Inversion de l'ordre d'affichage pour commencer en bas (au plus près de la zone de saisie)
            line.style.order = activeOthers.length - 1 - index;
          });

          // Handle nodes fading (33% -> 66% -> deletion) when no longer detected
          Array.from(container.children).forEach(line => {
            let id = line.id;
            if (!newIds.has(id)) {
              let fade = line.dataset.fadeLevel || "0";
              if (fade === "0") {
                line.dataset.fadeLevel = "33";
                line.style.opacity = "0.67";
              } else if (fade === "33") {
                line.dataset.fadeLevel = "66";
                line.style.opacity = "0.33";
              } else if (fade === "66") {
                line.remove();
              }
            } else {
              // Node is online, restore full opacity
              line.dataset.fadeLevel = "0";
              line.style.opacity = "1.0";
            }
          });


          processQueuedReplies();
        });
    };

    const couleurPermanente = (mac, isMe) => {
      if (isMe) return 'var(--primary)'; // gold color for ourselves to make our line distinct!
      return couleurDepuisMac(mac);
    };

    const showReactionsPicker = (event, targetMac) => {
      event.stopPropagation();
      currentVoteCible = targetMac;
      let picker = document.getElementById('popover-reactions');
      picker.style.display = 'flex';
      picker.style.left = `${Math.min(window.innerWidth - 180, event.clientX)}px`;
      picker.style.top = `${Math.max(10, event.clientY - 40)}px`;
      
      document.onclick = (e) => {
        if (!picker.contains(e.target)) {
          picker.style.display = 'none';
          document.onclick = null;
        }
      };
    };

    const vote = (emoji) => {
      if (!currentVoteCible) return;
      let fd = new URLSearchParams();
      fd.append('cible', currentVoteCible);
      fd.append('emoticon', emoji);
      fetch('/api/vote', { method: 'POST', body: fd })
        .then(() => {
          document.getElementById('popover-reactions').style.display = 'none';
          update();
        });
    };

    // "1.11" : pop-up lorsqu'un voisin "emoticone" un message
    const loadNotifications = () => {
      fetch('/api/notifications')
        .then(r => r.json())
        .then(data => {
          let area = document.getElementById('notifications-area');
          area.innerHTML = "";
          data.forEach(n => {
            if (n.age < 15000) {
              area.innerHTML += `
                <div class="notification-banner">
                  <span><strong>${n.voteur || "Quelqu'un"}</strong> a réagi avec ${n.emoticon} !</span>
                  <button onclick="this.parentElement.remove()">✖</button>
                </div>`;
            }
          });
        });
    };

    const loadReceivedGraffitis = () => {
      fetch('/api/messages')
        .then(r => r.json())
        .then(data => {
          let container = document.getElementById('received-graffitis-list');
          container.innerHTML = "";
          data = data.filter(g => {
            let author = g.author || "";
            let authorMac = "";
            if (author.includes("::")) {
              authorMac = author.split("::")[0].toLowerCase();
            } else {
              authorMac = findMacByUsername(author).toLowerCase();
            }
            return !myBanned.includes(authorMac);
          });
          if (data.length === 0) {
            container.innerHTML = `<div style="text-align:center; color:var(--text-muted); padding:20px; font-size:13px;">Aucun message pour le moment.</div>`;
            return;
          }
          data.reverse().forEach(g => {
            let card = document.createElement('div');
            card.className = "card";
            card.style.borderLeft = "4px solid var(--secondary)";
            card.style.textAlign = "left";
            card.style.position = "relative";
            
            let author = g.author || "Anonyme";
            let text = g.text || "";
            let index = g.index;

            let authorDisplay = author;
            let authorMac = "";
            if (author.includes("::")) {
              let parts = author.split("::");
              if (parts.length >= 2) {
                let isMac = /^[0-9a-fA-F]{12}$/.test(parts[0]);
                if (isMac) {
                  authorMac = parts[0];
                  authorDisplay = parts.slice(1).join("::");
                }
              }
            }
            if (!authorMac) {
              authorMac = findMacByUsername(authorDisplay);
            }
            
            card.innerHTML = `
              <button style="position:absolute; top:8px; right:8px; background:none; border:none; color:var(--text-muted); cursor:pointer; font-weight:bold; font-size:14px;" onclick="deleteGraffiti(${index})" title="Supprimer ce graffiti">✖</button>
              <div class="card-title" style="margin-bottom:5px; font-size:13px; padding-right:20px;">
                <span>De : <strong style="color:var(--secondary);">${authorDisplay}</strong></span>
              </div>
              <div style="font-size:14px; font-style:italic; line-height:1.4; color:var(--text); margin-bottom: 8px;">
                « ${text} »
              </div>
              <div style="display:flex; justify-content:flex-end;">
                <button class="btn btn-sec" style="padding:4px 8px; font-size:11px;" onclick="toggleReplyForm(${index}, '${authorMac}', '${authorDisplay}')">💬 Répondre</button>
              </div>
              <div id="reply-container-${index}" style="display:none; margin-top:10px; border-top:1px dashed var(--border); padding-top:10px;">
                <textarea id="reply-text-${index}" style="width:100%; height:60px; background:rgba(0,0,0,0.3); border:1px solid var(--border); color:var(--text); font-size:12px; resize:none; padding:8px; border-radius:6px; outline:none; font-family:inherit;" placeholder="Votre réponse à ${authorDisplay} (140 caractères max)..." maxlength="140"></textarea>
                <div style="display:flex; justify-content:space-between; align-items:center; margin-top:5px;">
                  <span id="reply-status-${index}" style="font-size:10px; color:var(--text-muted);">Vérification du statut...</span>
                  <div style="display:flex; gap:5px;">
                    <button class="btn btn-sec" style="padding:3px 6px; font-size:10px;" onclick="toggleReplyForm(${index})">Annuler</button>
                    <button class="btn" style="padding:3px 8px; font-size:10px;" onclick="submitReply(${index}, '${authorMac}', '${authorDisplay}')">Envoyer</button>
                  </div>
                </div>
              </div>
            `;
            container.appendChild(card);
          });
          markAllGraffitisAsSeen(data);
        });
    };

    const deleteGraffiti = (index) => {
      if (confirm("Supprimer ce graffiti ?")) {
        let fd = new URLSearchParams();
        fd.append('index', index);
        fetch('/api/delete_graffiti', { method: 'POST', body: fd })
          .then(() => loadReceivedGraffitis());
      }
    };

    const loadBroadcastHistory = () => {
      let list = document.getElementById('broadcast-history-list');
      if (!list) return;
      fetch('/api/history')
        .then(r => r.json())
        .then(data => {
          list.innerHTML = "";
          if (data.length === 0) {
            list.innerHTML = `<div style="text-align:center; color:var(--text-muted); padding:10px;">Aucun historique.</div>`;
            return;
          }
          data.reverse().forEach(h => {
            let div = document.createElement('div');
            div.style.display = "flex";
            div.style.flexDirection = "column";
            div.style.gap = "4px";
            div.style.padding = "6px 8px";
            div.style.background = "rgba(0,0,0,0.2)";
            div.style.borderRadius = "4px";
            div.style.border = "1px solid var(--border)";
            
            div.innerHTML = `
              <div style="display:flex; justify-content:space-between; font-size:10px; color:var(--text-muted);">
                <span>Diffusé le :</span>
                <span>${h.time}</span>
              </div>
              <div style="font-style:italic; color:var(--text); line-height:1.4;">
                « ${h.text} »
              </div>
            `;
            list.appendChild(div);
          });
        });
    };

    const clearHistory = () => {
      if (confirm("Effacer tout l'historique des diffusions ?")) {
        fetch('/api/clear_history', { method: 'POST' })
          .then(() => loadBroadcastHistory());
      }
    };

    const viewPeerCV = (mac, username) => {
      let viewer = document.getElementById('ov-profil-viewer');
      let content = document.getElementById('cv-viewer-content');
      
      content.innerHTML = `<h3>Chargement du profil de ${username}...</h3><div class="spinner"></div>`;
      viewer.classList.add('actif');
      
      let params = new URLSearchParams();
      params.append('mac', mac);
      fetch('/api/request_cv', { method: 'POST', body: params })
        .then(() => {
          let attempts = 0;
          let interval = setInterval(() => {
            attempts++;
            fetch('/api/get_cv?mac=' + encodeURIComponent(mac))
              .then(r => {
                if (r.status === 200) {
                  clearInterval(interval);
                  return r.text();
                }
                if (attempts > 12) {
                  clearInterval(interval);
                  throw new Error("Timeout");
                }
                return null;
              })
              .then(text => {
                if (text !== null) {
                  cvsStockes[mac] = text;
                  content.innerHTML = renderStructuredProfile(text, username, mac);
                }
              })
              .catch(err => {
                content.innerHTML = `<h3>Impossible de charger le profil de ${username}.</h3><p style="color:var(--text-muted); font-size:12px; text-align:center;">Le module est hors de portée ou n'a pas répondu.</p>`;
              });
          }, 1000);
        });
    };



    const renderStructuredProfile = (jsonStr, username, mac) => {
      let data = null;
      try {
        data = JSON.parse(jsonStr);
      } catch(e) {
        return `<h1>Profil de ${username}</h1><div class="line"></div>` + parseMarkdown(jsonStr);
      }

      let profile = data.profile || data;
      
      let headColor = profile.head_color || 'transparent';
      let torsoColor = profile.torso_color || '#e5e7eb';
      let torsoPattern = profile.torso_pattern || '';
      let legsColor = profile.legs_color || '#e5e7eb';
      let legsPattern = profile.legs_pattern || '';
      let feetColor = profile.feet_color || '#e5e7eb';
      
      let favText = myFavorites.includes(mac.toLowerCase()) ? "Retirer des favoris ⭐️" : "Ajouter aux favoris 🌟";

      let isUnset = Object.keys(profile).length === 0 || 
                    (headColor === 'transparent' && torsoColor === '#e5e7eb' && legsColor === '#e5e7eb' && feetColor === '#e5e7eb');

      let bodyMarkup = "";
      if (isUnset) {
        bodyMarkup = `
          <div style="display:flex; flex-direction:column; align-items:center; justify-content:center; background:var(--surface-low); border:1px solid var(--border); padding:30px; border-radius:8px; gap:10px;">
            <span style="font-size: 32px;">👤</span>
            <span style="color:var(--text-muted); font-size:13px; font-style:italic;">profil vestimentaire non renseigné</span>
          </div>
        `;
      } else {
        let headStyle = headColor === 'transparent' ? 'opacity:0.3; background-color:transparent;' : `background-color:${headColor};`;
        let torsoStyle = torsoColor === 'transparent' ? 'opacity:0.3; background-color:transparent;' : `background-color:${torsoColor};`;
        let legsStyle = legsColor === 'transparent' ? 'opacity:0.3; background-color:transparent;' : `background-color:${legsColor};`;
        let feetStyle = feetColor === 'transparent' ? 'opacity:0.3; background-color:transparent;' : `background-color:${feetColor};`;

        let torsoPatternClass = torsoPattern ? ` ${torsoPattern}` : '';
        let legsPatternClass = legsPattern ? ` ${legsPattern}` : '';

        bodyMarkup = `
          <div style="display:flex; align-items:center; justify-content:center; background:var(--surface-low); border:1px solid var(--border); padding:20px; border-radius:8px;">
            <div class="avatar-preview-box" style="background:rgba(0,0,0,0.3); border:none; margin:0 auto;">
              <div class="avatar-head" style="${headStyle}"></div>
              <div class="avatar-torso" style="${torsoStyle}">
                <div class="pattern-overlay${torsoPatternClass}"></div>
              </div>
              <div class="avatar-legs" style="${legsStyle}">
                <div class="pattern-overlay${legsPatternClass}"></div>
                <div class="avatar-leg-left"></div>
                <div class="avatar-leg-right"></div>
              </div>
              <div class="avatar-feet">
                <div class="avatar-foot" style="${feetStyle}"></div>
                <div class="avatar-foot" style="${feetStyle}"></div>
              </div>
            </div>
          </div>
        `;
      }

      let html = `
        <div style="text-align:center;">
          <h1 style="font-family:Georgia, serif; font-size:22px; color:var(--primary); margin-bottom:4px;">${username}</h1>
          <div style="font-size:11px; color:var(--text-muted); font-family:monospace; margin-bottom:15px;">MAC : ${mac}</div>
          
          <div style="display:flex; justify-content:center; gap:8px; margin-bottom:20px;">
            <button class="btn" style="padding:4px 10px; font-size:11px;" onclick="toggleFavoritePeer('${mac}', '${username}')">${favText}</button>
          </div>

          ${bodyMarkup}
        </div>
      `;
      return html;
    };

    const parseMarkdown = (md) => {
      if (!md || md.trim() === "") return "<em>Profil vide. Rédigez votre profil au format Markdown.</em>";
      return md
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/^# (.*?)$/gm, '<h1>$1</h1>')
        .replace(/^## (.*?)$/gm, '<h2>$1</h2>')
        .replace(/^### (.*?)$/gm, '<h3>$1</h3>')
        .replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>')
        .replace(/\*(.*?)\*/g, '<em>$1</em>')
        .replace(/^&gt;\s(.*?)$/gm, '<blockquote style="border-left: 3px solid var(--primary); padding-left: 10px; color: var(--text-muted); font-style: italic; margin: 10px 0;">$1</blockquote>')
        .replace(/^---$/gm, '<hr style="border:none; border-top:1px dashed var(--border); margin: 15px 0;">')
        .replace(/\[(.*?)\]\((.*?)\)/g, '<a href="$2" target="_blank" style="color:var(--secondary); text-decoration:underline;">$1</a>')
        .replace(/^\- (.*?)$/gm, '<li style="margin-left:20px; list-style-type:square;">$1</li>')
        .replace(/\n/g, '<br>');
    };



    const saveConfiguration = () => {
      let ssid = document.getElementById('config-ssid').value.trim();
      let pwd = document.getElementById('config-pwd').value;
      let username = document.getElementById('config-username').value.trim();
      let rebootMsg = document.getElementById('config-reboot-msg').value.trim();
      let mode = configSelectedMode;
      let rediffuseLast = document.getElementById('config-rediffuse-last').checked ? "1" : "0";

      if (ssid === "") return alert("Le SSID ne peut pas être vide !");
      let fd = new URLSearchParams();
      fd.append('ssid', ssid);
      fd.append('pwd', pwd);
      fd.append('username', username);
      fd.append('rebootMsg', rebootMsg);
      fd.append('mode', mode);
      fd.append('rediffuseLast', rediffuseLast);
      if (mode === 1) {
        localStorage.setItem('civvi_is_owner', 'true');
      } else {
        localStorage.removeItem('civvi_is_owner');
      }

      fetch('/api/config', { method: 'POST', body: fd })
        .then(() => {
          let newSSID = (mode == 1 || mode === "1") ? ssid + "-salon" : ssid;
          alert("Configuration sauvegardée ! L'ESP redémarre... Reconnectez-vous au réseau Wi-Fi : " + newSSID);
        });
    };

    const clearJournal = () => {
      if (confirm("Voulez-vous effacer tout le journal d'historique ?")) {
        fetch('/api/clear_journal', { method: 'POST' }).then(() => alert("Journal vidé !"));
      }
    };

    // Helper functions for DTN replies and badge counter

    const findMacByUsername = (username) => {
      if (!username) return "";
      let found = currentData.slice(1).find(m => {
        let u = (m.username && m.username.trim()) ? m.username : m.nomReseau;
        return u === username;
      });
      return found ? found.auteur : "";
    };

    window.toggleReplyForm = (index, mac, name) => {
      let container = document.getElementById(`reply-container-${index}`);
      if (!container) return;
      if (container.style.display === 'none') {
        container.style.display = 'block';
        let isOnline = currentData.slice(1).some(m => m.auteur.toLowerCase() === mac.toLowerCase() && m.age < 30000 && !expiredMacs.has(m.auteur));
        let statusSpan = document.getElementById(`reply-status-${index}`);
        if (statusSpan) {
          if (isOnline) {
            statusSpan.innerHTML = "🟢 En ligne (envoi direct)";
            statusSpan.style.color = "#10b981";
          } else {
            statusSpan.innerHTML = "💾 Hors ligne (mise en mémoire)";
            statusSpan.style.color = "var(--primary)";
          }
        }
      } else {
        container.style.display = 'none';
      }
    };

    window.submitReply = (index, mac, name) => {
      let textarea = document.getElementById(`reply-text-${index}`);
      if (!textarea || textarea.value.trim() === "") return;
      let text = textarea.value.trim();
      let fullMessage = "[Rép] " + text;
      if (fullMessage.length > 150) fullMessage = fullMessage.substring(0, 150);

      let isOnline = currentData.slice(1).some(m => m.auteur.toLowerCase() === mac.toLowerCase() && m.age < 30000 && !expiredMacs.has(m.auteur));
      if (isOnline) {
        let fd = new URLSearchParams();
        fd.append('cible', mac);
        fd.append('message', fullMessage);
        fetch('/api/remote_graffiti', { method: 'POST', body: fd })
          .then(() => {
            alert("Réponse envoyée directement à " + name + " !");
            window.toggleReplyForm(index);
            loadReceivedGraffitis();
          });
      } else {
        let queue = JSON.parse(localStorage.getItem('civvi_queued_replies') || '[]');
        queue.push({
          recipientMac: mac,
          recipientName: name,
          replyText: fullMessage,
          timestamp: Date.now()
        });
        localStorage.setItem('civvi_queued_replies', JSON.stringify(queue));
        alert("Réponse enregistrée en mémoire. Elle sera transmise automatiquement dès que " + name + " sera à portée.");
        window.toggleReplyForm(index);
      }
    };

    const processQueuedReplies = () => {
      let queue = JSON.parse(localStorage.getItem('civvi_queued_replies') || '[]');
      if (queue.length === 0) return;
      let updatedQueue = [];
      let sentCount = 0;
      queue.forEach(item => {
        let peer = currentData.slice(1).find(m => m.auteur.toLowerCase() === item.recipientMac.toLowerCase() && m.age < 30000 && !expiredMacs.has(m.auteur));
        if (peer) {
          let fd = new URLSearchParams();
          fd.append('cible', item.recipientMac);
          fd.append('message', item.replyText);
          fetch('/api/remote_graffiti', { method: 'POST', body: fd });
          let area = document.getElementById('notifications-area');
          if (area) {
            area.innerHTML += `
              <div class="notification-banner" style="background: rgba(16, 185, 129, 0.15); border-color: #10b981; border-left-color: #10b981;">
                <span><strong>[Mémoire]</strong> Réponse transmise automatiquement à ${item.recipientName} !</span>
                <button onclick="this.parentElement.remove()">✖</button>
              </div>`;
          }
          sentCount++;
        } else {
          updatedQueue.push(item);
        }
      });
      if (sentCount > 0) {
        localStorage.setItem('civvi_queued_replies', JSON.stringify(updatedQueue));
      }
    };

    const checkNewFeuilleMessages = () => {
      fetch('/api/messages')
        .then(r => r.json())
        .then(data => {
          let unseenCount = 0;
          data.forEach(g => {
            let key = g.author + "_" + g.text;
            if (!seenGraffitis.includes(key)) {
              unseenCount++;
            }
          });
          updateFeuilleBadge(unseenCount);
        });
    };

    const updateFeuilleBadge = (count) => {
      let badge = document.getElementById('badge-feuille');
      if (!badge) {
        badge = document.createElement('div');
        badge.id = 'badge-feuille';
        badge.className = 'badge-feuille';
        document.getElementById('btn-messagerie').appendChild(badge);
      }
      if (count > 0) {
        badge.innerText = count;
        badge.style.display = 'flex';
      } else {
        badge.style.display = 'none';
      }
    };

    const markAllGraffitisAsSeen = (data) => {
      let newSeen = [];
      data.forEach(g => {
        newSeen.push(g.author + "_" + g.text);
      });
      seenGraffitis = newSeen;
      localStorage.setItem('civvi_seen_graffitis', JSON.stringify(seenGraffitis));
      updateFeuilleBadge(0);
    };

      // Explicit Global Bindings
      window.selectAvatarColor = selectAvatarColor;
      window.selectAvatarPattern = selectAvatarPattern;
      window.applyAvatarState = applyAvatarState;
      window.resetAvatar = resetAvatar;
      window.toggleContrastMode = toggleContrastMode;
      window.switchToSalonMode = switchToSalonMode;
      window.confirmSalonUsername = confirmSalonUsername;
      window.switchBackToP2P = switchBackToP2P;
      window.insertMarkdown = insertMarkdown;
      window.saveMarkdownCV = saveMarkdownCV;
      window.viewPeerDoc = viewPeerDoc;
      window.toggleFavoritePeer = toggleFavoritePeer;
      window.toggleBannedPeer = toggleBannedPeer;
      window.loadFavoritesAndBannedUI = loadFavoritesAndBannedUI;
      window.saveStructuredProfile = saveStructuredProfile;
      window.loadStructuredProfile = loadStructuredProfile;
      window.ouvrirOverlay = ouvrirOverlay;
      window.fermerOverlays = fermerOverlays;
</script>
</body>
</html>
)rawliteral";


// CHAPITRE 7 : SÉCURITÉ, RECOMPILATION DES DÉCLARATIONS ET API SECONDAIRES
// Paragraphe 7.1 : En-têtes HTTP de non-cache
// Injecte des en-têtes HTTP empêchant la mise en cache des pages d'administration
void sendNoCacheHeaders() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
}


// Paragraphe 7.2 : API Utilitaires (Temps, historique et journalisation)
// Redirige vers la page d'accueil principale index.html
void handleRoot() {
  sendNoCacheHeaders();
  server.send_P(200, "text/html", htmlPage);
}

// Génère une chaîne contenant l'heure locale formatée "[JJ/MM HH:MM]"
String obtenirTempsFormate() {
  time_t now = time(nullptr);
  if (now < 1577836800) { 
    return "";
  }
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char buf[30];
  strftime(buf, sizeof(buf), "[%d/%m %H:%M]", &timeinfo);
  return String(buf);
}







// Enregistre un message émis dans l'historique local (history.txt)
void ajouterAHistory(String texte) {
  
  String t = obtenirTempsFormate();
  if (t.length() > 0) {
    t = t.substring(1, t.length() - 1); 
  } else {
    t = "--/-- --:--";
  }

  std::vector<String> entries;
  
  if (LittleFS.exists("/history.txt")) {
    File f = LittleFS.open("/history.txt", "r");
    if (f) {
      while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
          entries.push_back(line);
        }
      }
      f.close();
    }
  }

  
  String newLine = texte + "|||" + t;
  entries.push_back(newLine);

  
  size_t maxEntries = 50;
  if (entries.size() > maxEntries) {
    entries.erase(entries.begin(),
                  entries.begin() + (entries.size() - maxEntries));
  }

  
  File f = LittleFS.open("/history.txt", "w");
  if (f) {
    for (auto &entry : entries) {
      f.println(entry);
    }
    f.close();
  }
}

// Enregistre un message reçu dans le journal de bord (journal.txt)
void ajouterAuJournal(String auteur, String nomReseau, String texte) {
  String t = obtenirTempsFormate();
  if (t.length() > 0) {
    t = t.substring(1, t.length() - 1); 
  } else {
    t = "--/-- --:--";
  }

  std::vector<String> entries;
  if (LittleFS.exists("/journal.txt")) {
    File f = LittleFS.open("/journal.txt", "r");
    if (f) {
      while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
          entries.push_back(line);
        }
      }
      f.close();
    }
  }

  bool isDuplicate = false;
  for (const auto &entry : entries) {
    int s1 = entry.indexOf("|||");
    if (s1 > 0) {
      String entryAuteur = entry.substring(0, s1);
      if (entryAuteur == auteur) {
        int s2 = entry.indexOf("|||", s1 + 3);
        if (s2 > 0) {
          int s3 = entry.indexOf("|||", s2 + 3);
          String entryTexte =
              (s3 > 0) ? entry.substring(s2 + 3, s3) : entry.substring(s2 + 3);
          if (entryTexte == texte) {
            isDuplicate = true;
            break;
          }
        }
      }
    }
  }

  if (isDuplicate) {
    return;
  }

  String newLine = auteur + "|||" + nomReseau + "|||" + texte + "|||" + t;
  entries.push_back(newLine);

  size_t totalSpace = LittleFS.totalBytes();
  size_t usedSpace = LittleFS.usedBytes();
  size_t freeSpace = (totalSpace > usedSpace) ? (totalSpace - usedSpace) : 0;
  size_t maxEntries = 100;
  if (freeSpace < 32768) {
    maxEntries = 10;
  } else if (freeSpace < 65536) {
    maxEntries = 30;
  } else if (freeSpace < 131072) {
    maxEntries = 60;
  }

  if (entries.size() > maxEntries) {
    entries.erase(entries.begin(),
                  entries.begin() + (entries.size() - maxEntries));
  }

  File f = LittleFS.open("/journal.txt", "w");
  if (f) {
    for (auto &entry : entries) {
      f.println(entry);
    }
    f.close();
  }
}

// Paragraphe 7.3 : API de Vote et Notifications
// API synchronisant l'horloge système de l'ESP avec le client web
void handleTimeSync() {
  if (server.hasArg("time")) {
    long t = server.arg("time").substring(0, 10).toInt();
    long offset = server.arg("offset").toInt();

    struct timeval tv;
    tv.tv_sec = t + offset;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);

    server.send(200, "text/plain", "OK");
    Serial.print("Horloge ESP32 mise à jour : ");
    time_t now = time(nullptr);
    Serial.println(ctime(&now));
  } else {
    server.send(400, "text/plain", "Requête incorrecte");
  }
}

// API publiant un vote (réaction émoticône) vers un module voisin
void handlePostVote() {
  if (server.hasArg("cible") && server.hasArg("emoticon")) {
    String cible = server.arg("cible");
    String emoticon = server.arg("emoticon");

    String paquet = "V|" + cible + "|" + monID + "|" + emoticon;
    esp_now_send(broadcastAddress, (uint8_t *)paquet.c_str(), paquet.length());

    votesMap[cible][emoticon]++;

    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Paramètres manquants");
  }
}

// Paragraphe 7.4 : API d'Historique, Journal et Notes
// Recherche le pseudo d'un voisin à partir de sa MAC
String obtenirNomUtilisateurDepuisMac(String mac) {
  mac.toLowerCase();
  mac.trim();
  for (auto const &m : messages) {
    if (m.auteur == mac) {
      String nom = m.nomReseau;
      int sepIdx = nom.indexOf("::");
      if (sepIdx > 0) {
        return nom.substring(sepIdx + 2);
      }
      return nom;
    }
  }
  return mac;
}

// API renvoyant les notifications de votes récents
void handleGetNotifications() {
  String json = "[";
  for (int i = 0; i < voteNotifications.size(); i++) {
    if (i > 0)
      json += ",";
    String displayName = obtenirNomUtilisateurDepuisMac(voteNotifications[i].voteur);
    displayName.replace("\"", "\\\"");
    json +=
        "{\"voteur\":\"" + displayName + "\",\"emoticon\":\"" +
        voteNotifications[i].emoticon +
        "\",\"age\":" + String(millis() - voteNotifications[i].timeReceived) +
        "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

// API renvoyant le journal d'activité local au format HTML
void handleGetJournal() {
  if (!LittleFS.exists("/journal.txt")) {
    server.send(200, "application/json", "[]");
    return;
  }
  File f = LittleFS.open("/journal.txt", "r");
  if (!f) {
    server.send(500, "text/plain", "Erreur lecture journal");
    return;
  }

  String json = "[";
  bool first = true;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      String pieces[4];
      int count = 0;
      int startIdx = 0;
      while (startIdx < line.length() && count < 4) {
        int nextIdx = line.indexOf("|||", startIdx);
        if (nextIdx == -1) {
          pieces[count++] = line.substring(startIdx);
          break;
        }
        pieces[count++] = line.substring(startIdx, nextIdx);
        startIdx = nextIdx + 3;
      }
      if (count >= 3) {
        if (!first)
          json += ",";
        String auteur = pieces[0];
        String nom = pieces[1];
        String texte = pieces[2];
        String timestamp = (count >= 4) ? pieces[3] : "";

        auteur.replace("\"", "\\\"");
        nom.replace("\"", "\\\"");
        texte.replace("\"", "\\\"");

        json += "{\"author\":\"" + auteur + "\",\"ssid\":\"" + nom +
                "\",\"text\":\"" + texte + "\",\"time\":\"" + timestamp + "\"}";
        first = false;
      }
    }
  }
  f.close();
  json += "]";
  server.send(200, "application/json", json);
}

// API réinitialisant (effaçant) le journal local
void handleClearJournal() {
  if (LittleFS.exists("/journal.txt")) {
    LittleFS.remove("/journal.txt");
  }
  server.send(200, "text/plain", "OK");
}



// API renvoyant l'historique des diffusions au format JSON
void handleGetHistory() {
  if (!LittleFS.exists("/history.txt")) {
    server.send(200, "application/json", "[]");
    return;
  }
  File f = LittleFS.open("/history.txt", "r");
  if (!f) {
    server.send(500, "text/plain", "Erreur lecture historique");
    return;
  }

  String json = "[";
  bool first = true;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      int sep = line.indexOf("|||");
      if (sep > 0) {
        if (!first)
          json += ",";
        String txt = line.substring(0, sep);
        int sep2 = line.indexOf("|||", sep + 3);
        String timestamp;
        String emos = "";
        if (sep2 > 0) {
          timestamp = line.substring(sep + 3, sep2);
          emos = line.substring(sep2 + 3);
        } else {
          timestamp = line.substring(sep + 3);
        }
        
        txt.replace("\"", "\\\"");
        json += "{\"text\":\"" + txt + "\",\"time\":\"" + timestamp + "\",\"emoticons\":\"" + emos + "\"}";
        first = false;
      }
    }
  }
  f.close();
  json += "]";
  server.send(200, "application/json", json);
}


// API effaçant l'historique local
void handleClearHistory() {
  if (LittleFS.exists("/history.txt")) {
    LittleFS.remove("/history.txt");
  }
  server.send(200, "text/plain", "OK");
}

// API envoyant notre pensée/message principal rédigé vers le réseau local
void handleSend() {
  if (server.hasArg("plain")) {
    String messageText = server.arg("plain");
    if (fonctionnementMode == 1) { 
      String username = server.hasArg("user") ? server.arg("user") : "Anonyme";
      username.trim();
      if (username.length() == 0) username = "Anonyme";
      
      String timeStr = obtenirTempsFormate();
      if (timeStr.length() > 0) {
        if (!messageText.startsWith("[")) {
          messageText = timeStr + " " + messageText;
        }
      }

      SalonMessage sm;
      sm.auteur = username;
      sm.texte = messageText;
      sm.timestamp = millis();

      salonMessages.push_back(sm);
      if (salonMessages.size() > 40) {
        salonMessages.erase(salonMessages.begin());
      }
    } else { 
      String timeStr = obtenirTempsFormate();
      if (timeStr.length() > 0) {
        if (!messageText.startsWith("[")) {
          messageText = timeStr + " " + messageText;
        }
      }
      maPensee = messageText;
      votesMap[monID].clear();
      sauvegarderPensees();
      ajouterAuJournal(monNomUtilisateur, monNomReseau, maPensee);
      ajouterAHistory(maPensee);
      envoyerTexteLong();
    }
  }
  server.send(200, "text/plain", "OK");
}

// API renvoyant les messages du réseau pour l'interface principale
void handleMessages() {
  if (fonctionnementMode == 1) { 
    String json = "[";
    String hostSSID = monNomReseau + "-salon";
    json += "{\"auteur\":\"host\",\"nomReseau\":\"" + hostSSID + "\",\"username\":\"Salon\",\"texte\":\"\",\"rssi\":0,\"age\":0,\"contrast\":0}";
    
    for (int i = (int)salonMessages.size() - 1; i >= 0; i--) {
      json += ",";
      String escapedText = salonMessages[i].texte;
      escapedText.replace("\"", "\\\"");
      escapedText.replace("\n", "\\n");
      escapedText.replace("\r", "");
      String escapedAuteur = salonMessages[i].auteur;
      escapedAuteur.replace("\"", "\\\"");
      
      String uniqueAuteurId = escapedAuteur + "_" + String(salonMessages[i].timestamp);
      
      json += "{\"auteur\":\"" + uniqueAuteurId + "\",\"nomReseau\":\"Salon\",\"username\":\"" + escapedAuteur + "\",\"texte\":\"" + escapedText + "\",\"rssi\":0,\"age\":0,\"contrast\":0}";
    }
    json += "]";
    server.send(200, "application/json", json);
    return;
  }

  String myVotesJson = "{";
  if (votesMap.find(monID) != votesMap.end()) {
    bool firstVote = true;
    for (auto const &[emo, val] : votesMap[monID]) {
      if (!firstVote)
        myVotesJson += ",";
      myVotesJson += "\"" + emo + "\":" + String(val);
      firstVote = false;
    }
  }
  myVotesJson += "}";

  String escapedMyPensee = maPensee;
  escapedMyPensee.replace("\"", "\\\"");
  escapedMyPensee.replace("\n", "\\n");
  escapedMyPensee.replace("\r", "");

  String json = "[";
  json += "{\"auteur\":\"" + monID + "\",\"nomReseau\":\"" + monNomReseau +
          "\",\"username\":\"" + monNomUtilisateur + "\",\"texte\":\"" +
          escapedMyPensee + "\",\"rssi\":0,\"age\":0,\"votes\":" + myVotesJson + 
          ",\"contrast\":" + String(monContraste) + "}";

  for (int i = 0; i < messages.size(); i++) {
    String ssidPart = messages[i].nomReseau;
    String userPart = messages[i].nomReseau;
    int sepIdx = messages[i].nomReseau.indexOf("::");
    if (sepIdx > 0) {
      ssidPart = messages[i].nomReseau.substring(0, sepIdx);
      userPart = messages[i].nomReseau.substring(sepIdx + 2);
    }

    unsigned long age = millis() - messages[i].dernierContact;

    String votesJson = "{";
    if (votesMap.find(messages[i].auteur) != votesMap.end()) {
      bool firstVote = true;
      for (auto const &[emo, val] : votesMap[messages[i].auteur]) {
        if (!firstVote)
          votesJson += ",";
        votesJson += "\"" + emo + "\":" + String(val);
        firstVote = false;
      }
    }
    votesJson += "}";

    String escapedPeerText = messages[i].texte;
    escapedPeerText.replace("\"", "\\\"");
    escapedPeerText.replace("\n", "\\n");
    escapedPeerText.replace("\r", "");

    json += ",{\"auteur\":\"" + messages[i].auteur + "\",\"nomReseau\":\"" +
            ssidPart + "\",\"username\":\"" + userPart + "\",\"texte\":\"" +
            escapedPeerText + "\",\"rssi\":" + String(messages[i].rssi) +
            ",\"age\":" + String(age) + ",\"votes\":" + votesJson + 
            ",\"contrast\":" + String(messages[i].contrast) + "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

// API renvoyant les notes stockées dans LittleFS
void handleGetNotes() {
  if (LittleFS.exists("/notes.md")) {
    File file = LittleFS.open("/notes.md", "r");
    server.streamFile(file, "text/plain");
    file.close();
  } else {
    server.send(200, "text/plain", "");
  }
}

// API enregistrant les notes rédigées dans LittleFS
void handlePostNotes() {
  if (server.hasArg("plain")) {
    File file = LittleFS.open("/notes.md", "w");
    if (file) {
      file.print(server.arg("plain"));
      file.close();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(500, "text/plain", "Erreur fichier");
    }
  } else {
    server.send(400, "text/plain", "Vide");
  }
}

// API gérant les erreurs 404 (ressources Web manquantes)
void handleNotFound() {
  String path = server.uri();
  if (LittleFS.exists(path)) {
    handleStaticFile(path, false);
    return;
  }
  server.send(404, "text/plain", "404: Not Found");
}


// CHAPITRE 8 : INITIALISATION DE L'ESP (SETUP) ET COMPORTEMENT CYCLIQUE (LOOP)
// Paragraphe 8.1 : setup (Configuration matérielle, WiFi, ESP-NOW et Serveur Web)
// Fonction d'initialisation appelée une seule fois au boot de l'ESP
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n--- Initialisation Civvi Hybride v11 ---");

  initFS();


  if (!LittleFS.exists("/site")) LittleFS.mkdir("/site");
  {
    File f = LittleFS.open("/index.html", "w");
    if (f) {
      f.print(htmlPage);
      f.close();
      Serial.println("Fichier /index.html mis a jour en local");
    }
  }
  chargerConfig();

  
  
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  String activeSSID = monNomReseau;
  if (fonctionnementMode == 1) {
    activeSSID += "-salon";
  }

  if (monMotDePasse.length() >= 8) {
    WiFi.softAP(activeSSID.c_str(), monMotDePasse.c_str(), 1);
  } else {
    WiFi.softAP(activeSSID.c_str(), "", 1);
  }

  Serial.print("Réseau créé : ");
  Serial.println(activeSSID);

  
  if (esp_wifi_set_promiscuous(true) != ESP_OK)
    Serial.println("Erreur mode promiscuous");
  if (esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE) != ESP_OK)
    Serial.println("Erreur configuration canal");
  esp_wifi_set_promiscuous(false);

  monID = WiFi.macAddress();
  monID.replace(":", "");
  monID.toLowerCase();
  chargerPensees();

  queueMutex = xSemaphoreCreateMutex();

  if (esp_now_init() == ESP_OK) {
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
    esp_now_register_recv_cb(onReceive);
  }

  
  server.on("/", HTTP_GET, handleRoot);

  
  server.on("/send", HTTP_POST, handleSend);
  server.on("/messages", HTTP_GET, handleMessages);

  
  server.on("/notes", HTTP_GET, handleGetNotes);
  server.on("/notes", HTTP_POST, handlePostNotes);

  
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);

  server.on("/api/set_mode", HTTP_POST, []() {
    if (server.hasArg("mode")) {
      int nouveauMode = server.arg("mode").toInt();
      File fTemp = LittleFS.open("/mode_change.txt", "w");
      if (fTemp) {
        fTemp.println(String(nouveauMode));
        fTemp.close();
      }
      fonctionnementMode = nouveauMode;
      if (nouveauMode == 0 && monNomUtilisateurP2P.length() > 0) {
        monNomUtilisateur = monNomUtilisateurP2P;
      }
      File f = LittleFS.open(CONFIG_FILE, "w");
      if (f) {
        f.println(monNomReseau);
        f.println(monMotDePasse);
        f.println(monMessageReboot);
        f.println(monNomUtilisateur);
        f.println(String(fonctionnementMode));
        f.println(String(rediffuserDernier));
        f.println(monNomUtilisateurP2P);
        f.close();
        server.send(200, "text/plain", "OK");
        delay(500);
        ESP.restart();
      } else {
        server.send(500, "text/plain", "Erreur écriture");
      }
    } else {
      server.send(400, "text/plain", "Paramètre manquant");
    }
  });

  server.on("/api/my_contrast", HTTP_POST, []() {
    if (server.hasArg("contrast")) {
      monContraste = server.arg("contrast").toInt();
      File f = LittleFS.open("/contrast.txt", "w");
      if (f) {
        f.print(String(monContraste));
        f.close();
      }
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Missing contrast");
    }
  });

  server.on("/api/trigger_broadcast", HTTP_POST, []() {
    envoyerTexteLong();
    server.send(200, "text/plain", "OK");
  });

  server.on("/api/favorites", HTTP_GET, []() {
    String json = "[";
    for (size_t i = 0; i < favorites.size(); i++) {
      if (i > 0) json += ",";
      json += "\"" + favorites[i] + "\"";
    }
    json += "]";
    server.send(200, "application/json", json);
  });

  server.on("/api/favorites/toggle", HTTP_POST, []() {
    if (server.hasArg("mac")) {
      String mac = server.arg("mac");
      mac.toLowerCase();
      mac.trim();
      bool found = false;
      for (auto it = favorites.begin(); it != favorites.end(); ++it) {
        if (*it == mac) {
          favorites.erase(it);
          found = true;
          break;
        }
      }
      if (!found) favorites.push_back(mac);
      File f = LittleFS.open("/favorites.txt", "w");
      if (f) {
        for (String m : favorites) f.println(m);
        f.close();
      }
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Missing MAC");
    }
  });

  server.on("/api/banned", HTTP_GET, []() {
    String json = "[";
    for (size_t i = 0; i < banned.size(); i++) {
      if (i > 0) json += ",";
      json += "\"" + banned[i] + "\"";
    }
    json += "]";
    server.send(200, "application/json", json);
  });

  server.on("/api/banned/toggle", HTTP_POST, []() {
    if (server.hasArg("mac")) {
      String mac = server.arg("mac");
      mac.toLowerCase();
      mac.trim();
      bool found = false;
      for (auto it = banned.begin(); it != banned.end(); ++it) {
        if (*it == mac) {
          banned.erase(it);
          found = true;
          break;
        }
      }
      if (!found) banned.push_back(mac);
      File f = LittleFS.open("/banned.txt", "w");
      if (f) {
        for (String m : banned) f.println(m);
        f.close();
      }
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Missing MAC");
    }
  });

  server.on("/api/profile", HTTP_GET, handleGetProfile);
  server.on("/api/profile", HTTP_POST, handlePostProfile);

  server.on("/api/get_cv", HTTP_GET, []() {
    if (server.hasArg("mac")) {
      String targetMac = server.arg("mac");
      targetMac.toLowerCase();
      targetMac.trim();
      if (cvsStockes.find(targetMac) != cvsStockes.end()) {
        server.send(200, "text/plain", cvsStockes[targetMac]);
      } else {
        server.send(202, "text/plain", "Pending");
      }
    } else {
      server.send(400, "text/plain", "Missing MAC");
    }
  });

  server.on("/api/request_cv", HTTP_POST, []() {
    if (server.hasArg("mac")) {
      String targetMac = server.arg("mac");
      targetMac.toLowerCase();
      targetMac.trim();
      String paquet = "R|" + targetMac + "|" + monID;
      esp_now_send(broadcastAddress, (uint8_t *)paquet.c_str(), paquet.length());
      server.send(200, "text/plain", "Requested");
    } else {
      server.send(400, "text/plain", "Missing MAC");
    }
  });

  server.on("/api/get_doc", HTTP_GET, []() {
    if (server.hasArg("mac")) {
      String targetMac = server.arg("mac");
      targetMac.toLowerCase();
      targetMac.trim();
      if (docsStockes.find(targetMac) != docsStockes.end()) {
        server.send(200, "text/plain", docsStockes[targetMac]);
      } else {
        server.send(202, "text/plain", "Pending");
      }
    } else {
      server.send(400, "text/plain", "Missing MAC");
    }
  });

  server.on("/api/request_doc", HTTP_POST, []() {
    if (server.hasArg("mac")) {
      String targetMac = server.arg("mac");
      targetMac.toLowerCase();
      targetMac.trim();
      String paquet = "Q|" + targetMac + "|" + monID;
      esp_now_send(broadcastAddress, (uint8_t *)paquet.c_str(), paquet.length());
      server.send(200, "text/plain", "Requested");
    } else {
      server.send(400, "text/plain", "Missing MAC");
    }
  });

  



  
  server.on("/api/messages", HTTP_GET, handleGetGraffitis);
  server.on("/api/messages", HTTP_POST, handlePostGraffiti);
  server.on("/api/remote_graffiti", HTTP_POST, handleRemoteGraffiti);
  
  server.on("/api/sync-time", HTTP_POST, handleTimeSync);
  server.on("/synchro-temps", HTTP_POST, handleTimeSync);
  server.on("/api/vote", HTTP_POST, handlePostVote);
  server.on("/api/notifications", HTTP_GET, handleGetNotifications);



  server.on("/api/journal", HTTP_GET, handleGetJournal);
  server.on("/api/clear_journal", HTTP_POST, handleClearJournal);
  server.on("/api/clear_journal", HTTP_DELETE, handleClearJournal);
  server.on("/api/delete_graffiti", HTTP_POST, handleDeleteGraffiti);
  server.on("/api/history", HTTP_GET, handleGetHistory);
  server.on("/api/clear_history", HTTP_POST, handleClearHistory);

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Serveur prêt. Connectez-vous et rendez-vous sur 192.168.4.1");
}


// Traite la file d'attente interne des paquets radio reçus asynchrones
void traiterPaquetsRecus() {
  while (true) {
    PaquetRecu pr;
    bool hasPacket = false;

    if (queueMutex != NULL && xSemaphoreTake(queueMutex, portMAX_DELAY) == pdTRUE) {
      if (!paquetsRecusQueue.empty()) {
        pr = paquetsRecusQueue.front();
        paquetsRecusQueue.erase(paquetsRecusQueue.begin());
        hasPacket = true;
      }
      xSemaphoreGive(queueMutex);
    }

    if (!hasPacket) {
      break;
    }

    traiterPaquetRecu(pr.msg, pr.rssi, pr.senderMac);
  }
}

// Paragraphe 8.2 : loop (Traitement cyclique des paquets radio et des files d'attente)
// Boucle d'exécution principale continue du système
void loop() {
  traiterPaquetsRecus();
  server.handleClient();

  if (!cvRequestsQueue.empty()) {
    CVRequest req = cvRequestsQueue.back();
    cvRequestsQueue.pop_back();
    envoyerCV(req.targetMac, req.requesterMac);
  }

  if (!docRequestsQueue.empty()) {
    CVRequest req = docRequestsQueue.back();
    docRequestsQueue.pop_back();
    envoyerDoc(req.targetMac, req.requesterMac);
  }





  static unsigned long derniereRequeteAttente = 0;
  if (millis() - derniereRequeteAttente > 15000) {
    derniereRequeteAttente = millis();
    String paquet = "RQ|" + monID;
    esp_now_send(broadcastAddress, (uint8_t *)paquet.c_str(), paquet.length());
  }

  if (fonctionnementMode == 0) { 
    
    bool fsAChange = false;
    for (int i = 0; i < messages.size(); i++) {
      if (millis() - messages[i].dernierContact > TIMEOUT_MS) {
        messages.erase(messages.begin() + i);
        i--;
        fsAChange = true;
      }
    }

    if (fsAChange || resauvegarder) {
      sauvegarderPensees();
      resauvegarder = false;
    }

    for (auto it = messagesEnAttente.begin(); it != messagesEnAttente.end();) {
      if (millis() - it->second.dernierUpdate > 10000) {
        it = messagesEnAttente.erase(it);
      } else {
        ++it;
      }
    }

    for (auto it = cvsEnAttente.begin(); it != cvsEnAttente.end();) {
      if (millis() - it->second.dernierUpdate > 10000) {
        it = cvsEnAttente.erase(it);
      } else {
        ++it;
      }
    }

    for (auto it = docsEnAttente.begin(); it != docsEnAttente.end();) {
      if (millis() - it->second.dernierUpdate > 10000) {
        it = docsEnAttente.erase(it);
      } else {
        ++it;
      }
    }







    if (millis() - dernierEnvoi > 10000) {
      envoyerTexteLong();
      dernierEnvoi = millis();
    }
  }
}
