#include <LittleFS.h> // Inclusion de la bibliotheque standard <LittleFS.h> pour les dependances materielles ou logicielles.
#include <WebServer.h> // Inclusion de la bibliotheque standard <WebServer.h> pour les dependances materielles ou logicielles.
#include <WiFi.h> // Inclusion de la bibliotheque standard <WiFi.h> pour les dependances materielles ou logicielles.
#include <esp_now.h> // Inclusion de la bibliotheque standard <esp_now.h> pour les dependances materielles ou logicielles.
#include <esp_wifi.h> // Inclusion de la bibliotheque standard <esp_wifi.h> pour les dependances materielles ou logicielles.
#include <map> // Inclusion de la bibliotheque standard <map> pour les dependances materielles ou logicielles.
#include <sys/time.h> // Inclusion de la bibliotheque standard <sys/time.h> pour les dependances materielles ou logicielles.
#include <time.h> // Inclusion de la bibliotheque standard <time.h> pour les dependances materielles ou logicielles.
#include <vector> // Inclusion de la bibliotheque standard <vector> pour les dependances materielles ou logicielles.

// ===== CONFIGURATION CIVVI ===== // Affectation de valeur a une variable ou modification d'etat.
String monNomReseau = "civvi-do";            // ✏️ Le nom de ce relais (et l'auteur des graffitis) // Affectation de valeur a une variable ou modification d'etat.
String monMotDePasse = ""; // ✏️ Mot de passe Wi-Fi (vide = réseau ouvert) // Affectation de valeur a une variable ou modification d'etat.
String monMessageReboot = "";  // ✏️ Message de reboot par défaut // Affectation de valeur a une variable ou modification d'etat.
String monNomUtilisateur = ""; // ✏️ Nom d'utilisateur (auteur affiché) // Affectation de valeur a une variable ou modification d'etat.

// Struct pour le salon local // Instruction d'execution.
struct SalonMessage { // Instruction d'execution.
  String auteur; // Instruction d'execution.
  String texte; // Instruction d'execution.
  unsigned long timestamp; // Instruction d'execution.
}; // Instruction d'execution.
std::vector<SalonMessage> salonMessages; // Tableau dynamique C++ stockant les donnees du programme.
int fonctionnementMode = 0; // 0 = Relais (P2P), 1 = Salon (Centralisé) // Affectation de valeur a une variable ou modification d'etat.

IPAddress apIP(192, 168, 4, 1); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
WebServer server(80); // Declaration du serveur web HTTP sur le port 80.

// ===== TELEPROMPTEUR (Pensées) ===== // Affectation de valeur a une variable ou modification d'etat.
String maPensee = ""; // Affectation de valeur a une variable ou modification d'etat.
String monID = ""; // Variable contenant l'identifiant unique (MAC) du module.
uint32_t monMsgID = 0; // Affectation de valeur a une variable ou modification d'etat.

#define MAX_MESSAGES 144 // Definition de la constante preprocesseur MAX_MESSAGES avec la valeur 144.
#define TIMEOUT_MS 180000    // 3 minutes // Definition de la constante preprocesseur TIMEOUT_MS avec la valeur 180000.
#define MAX_PAYLOAD_SIZE 120 // Taille max d'un morceau // Definition de la constante preprocesseur MAX_PAYLOAD_SIZE avec la valeur 120.
#define MAX_CHUNKS 50        // 35 morceaux * 180 = 6300 caractères // Definition de la constante preprocesseur MAX_CHUNKS avec la valeur 50.

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Affectation de valeur a une variable ou modification d'etat.
esp_now_peer_info_t peerInfo; // Instruction d'execution.

struct Message { // Instruction d'execution.
  String auteur; // Instruction d'execution.
  String nomReseau; // Instruction d'execution.
  String texte; // Instruction d'execution.
  int rssi; // Instruction d'execution.
  unsigned long dernierContact; // Instruction d'execution.
  int contrast; // Instruction d'execution.
}; // Instruction d'execution.

struct MessageIncomplet { // Instruction d'execution.
  String nomReseau; // Instruction d'execution.
  int total; // Instruction d'execution.
  int recu; // Instruction d'execution.
  int rssi; // Instruction d'execution.
  String morceaux[MAX_CHUNKS]; // Instruction d'execution.
  unsigned long dernierUpdate; // Instruction d'execution.
  int contrast; // Instruction d'execution.
}; // Instruction d'execution.

struct VoteNotification { // Instruction d'execution.
  String voteur; // Instruction d'execution.
  String emoticon; // Instruction d'execution.
  unsigned long timeReceived; // Instruction d'execution.
}; // Instruction d'execution.

std::vector<Message> messages; // Tableau dynamique C++ stockant les donnees du programme.
std::map<String, MessageIncomplet> messagesEnAttente; // Instruction d'execution.
std::vector<VoteNotification> voteNotifications; // Tableau dynamique C++ stockant les donnees du programme.
std::vector<String> favorites; // Tableau dynamique C++ stockant les donnees du programme.
std::vector<String> banned; // Tableau dynamique C++ stockant les donnees du programme.
struct CVRequest { // Instruction d'execution.
  String targetMac; // Instruction d'execution.
  String requesterMac; // Instruction d'execution.
}; // Instruction d'execution.
std::vector<CVRequest> cvRequestsQueue; // Tableau dynamique C++ stockant les donnees du programme.
int monContraste = 0; // Affectation de valeur a une variable ou modification d'etat.
std::map<String, std::map<String, int>> votesMap; // MAC -> (emoticon -> count) // Appel d'une fonction interne ou d'une memoire de bibliotheque.
std::map<String, MessageIncomplet> cvsEnAttente; // Instruction d'execution.
std::map<String, String> cvsStockes; // Instruction d'execution.

unsigned long dernierEnvoi = 0; // Affectation de valeur a une variable ou modification d'etat.
volatile bool resauvegarder = false; // Affectation de valeur a une variable ou modification d'etat.

// ===== MUR DE GRAFFITIS ===== // Affectation de valeur a une variable ou modification d'etat.
const int MAX_GRAFFITIS = 50; // Instruction d'execution.
const char *GRAFFITIS_FILE = "/messages.txt"; // Instruction d'execution.
const char *PENSEES_FILE = "/pensees.txt"; // Instruction d'execution.

// ===== FONCTIONS DE MÉMOIRE (LITTLEFS) ===== // Affectation de valeur a une variable ou modification d'etat.
const char *CONFIG_FILE = "/config.txt"; // Instruction d'execution.

// Forward declarations // Instruction d'execution.
void ajouterAuJournal(String auteur, String nomReseau, String texte); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
void ajouterAHistory(String texte); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
String obtenirTempsFormate(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

bool estBanni(String mac) { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  mac.toLowerCase(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  mac.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  for (String m : banned) { // Execution d'une boucle iterative.
    if (m == mac) return true; // Verification d'une condition logique.
  }
  return false; // Instruction d'execution.
}

bool estFavori(String mac) { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  mac.toLowerCase(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  mac.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  for (String m : favorites) { // Execution d'une boucle iterative.
    if (m == mac) return true; // Verification d'une condition logique.
  }
  return false; // Instruction d'execution.
}

void chargerListes() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  favorites.clear(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (LittleFS.exists("/favorites.txt")) { // Verification d'une condition logique.
    File f = LittleFS.open("/favorites.txt", "r"); // Gestion des fichiers internes de stockage LittleFS.
    while (f.available()) { // Execution d'une boucle iterative.
      String line = f.readStringUntil('\n'); // Affectation de valeur a une variable ou modification d'etat.
      line.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      line.toLowerCase(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      if (line.length() > 0) { // Verification d'une condition logique.
        favorites.push_back(line); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      }
    }
    f.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  }
  
  banned.clear(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (LittleFS.exists("/banned.txt")) { // Verification d'une condition logique.
    File f = LittleFS.open("/banned.txt", "r"); // Gestion des fichiers internes de stockage LittleFS.
    while (f.available()) { // Execution d'une boucle iterative.
      String line = f.readStringUntil('\n'); // Affectation de valeur a une variable ou modification d'etat.
      line.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      line.toLowerCase(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      if (line.length() > 0) { // Verification d'une condition logique.
        banned.push_back(line); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      }
    }
    f.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  }
}

void handleGetProfile() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (LittleFS.exists("/profile.json")) { // Verification d'une condition logique.
    File f = LittleFS.open("/profile.json", "r"); // Gestion des fichiers internes de stockage LittleFS.
    server.streamFile(f, "application/json"); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    f.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  } else { // Instruction d'execution.
    server.send(200, "application/json", "{}"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
  }
}

void handlePostProfile() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (server.hasArg("plain")) { // Verification d'une condition logique.
    String body = server.arg("plain"); // Affectation de valeur a une variable ou modification d'etat.
    File f = LittleFS.open("/profile.json", "w"); // Gestion des fichiers internes de stockage LittleFS.
    if (f) { // Verification d'une condition logique.
      f.print(body); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      f.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      server.send(200, "text/plain", "OK"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    } else { // Instruction d'execution.
      server.send(500, "text/plain", "Error writing profile"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    }
  } else { // Instruction d'execution.
    server.send(400, "text/plain", "Body missing"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
  }
}

void chargerConfig() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (LittleFS.exists(CONFIG_FILE)) { // Verification d'une condition logique.
    File f = LittleFS.open(CONFIG_FILE, "r"); // Gestion des fichiers internes de stockage LittleFS.
    if (f) { // Verification d'une condition logique.
      String ssid = f.readStringUntil('\n'); // Affectation de valeur a une variable ou modification d'etat.
      String pwd = f.readStringUntil('\n'); // Affectation de valeur a une variable ou modification d'etat.
      String rebootMsg = f.readStringUntil('\n'); // Affectation de valeur a une variable ou modification d'etat.
      String username = f.readStringUntil('\n'); // Affectation de valeur a une variable ou modification d'etat.
      String modeStr = f.readStringUntil('\n'); // Affectation de valeur a une variable ou modification d'etat.
      ssid.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      pwd.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      rebootMsg.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      username.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      modeStr.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      if (ssid.length() > 0) { // Verification d'une condition logique.
        monNomReseau = ssid; // Affectation de valeur a une variable ou modification d'etat.
        monMotDePasse = pwd; // Affectation de valeur a une variable ou modification d'etat.
      }
      if (rebootMsg.length() > 0) { // Verification d'une condition logique.
        monMessageReboot = rebootMsg; // Affectation de valeur a une variable ou modification d'etat.
      }
      if (username.length() > 0) { // Verification d'une condition logique.
        monNomUtilisateur = username; // Affectation de valeur a une variable ou modification d'etat.
      } else { // Instruction d'execution.
        monNomUtilisateur = monNomReseau; // Affectation de valeur a une variable ou modification d'etat.
      }
      if (modeStr.length() > 0) { // Verification d'une condition logique.
        fonctionnementMode = modeStr.toInt(); // Affectation de valeur a une variable ou modification d'etat.
      } else { // Instruction d'execution.
        fonctionnementMode = 0; // Affectation de valeur a une variable ou modification d'etat.
      }
      f.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    }
  }
  if (monNomUtilisateur.length() == 0) { // Verification d'une condition logique.
    monNomUtilisateur = monNomReseau; // Affectation de valeur a une variable ou modification d'etat.
  }
}

void initFS() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (!LittleFS.begin(true)) { // Verification d'une condition logique.
    Serial.println("Erreur: Impossible de monter LittleFS"); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    return; // Instruction d'execution.
  }
  Serial.println("LittleFS monté avec succès"); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

  if (!LittleFS.exists(GRAFFITIS_FILE)) { // Verification d'une condition logique.
    File file = LittleFS.open(GRAFFITIS_FILE, "w"); // Gestion des fichiers internes de stockage LittleFS.
    if (file) { // Verification d'une condition logique.
      file.print(""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      file.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      Serial.println("Fichier de graffitis créé."); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    }
  }
}

void sauvegarderPensees() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  File f = LittleFS.open(PENSEES_FILE, FILE_WRITE); // Gestion des fichiers internes de stockage LittleFS.
  if (!f) // Verification d'une condition logique.
    return; // Instruction d'execution.
  // Sauvegarde uniquement notre propre pensée pour éviter de réimporter de // Instruction d'execution.
  // vieux peers au reboot // Instruction d'execution.
  f.println(monID + "|" + monNomReseau + "|" + maPensee); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  f.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
}

void chargerPensees() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (monMessageReboot.length() > 0) { // Verification d'une condition logique.
    maPensee = monMessageReboot; // Affectation de valeur a une variable ou modification d'etat.
  }

  if (!LittleFS.exists(PENSEES_FILE)) // Verification d'une condition logique.
    return; // Instruction d'execution.
  File f = LittleFS.open(PENSEES_FILE, FILE_READ); // Gestion des fichiers internes de stockage LittleFS.
  while (f.available()) { // Execution d'une boucle iterative.
    String ligne = f.readStringUntil('\n'); // Affectation de valeur a une variable ou modification d'etat.
    ligne.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    int s1 = ligne.indexOf('|'); // Affectation de valeur a une variable ou modification d'etat.
    int s2 = ligne.indexOf('|', s1 + 1); // Affectation de valeur a une variable ou modification d'etat.
    if (s1 > 0 && s2 > 0) { // Verification d'une condition logique.
      String auteur = ligne.substring(0, s1); // Affectation de valeur a une variable ou modification d'etat.
      String nom = ligne.substring(s1 + 1, s2); // Affectation de valeur a une variable ou modification d'etat.
      String texte = ligne.substring(s2 + 1); // Affectation de valeur a une variable ou modification d'etat.
      if (auteur == monID) { // Verification d'une condition logique.
        // Le message par défaut au reboot a priorité s'il est configuré // Instruction d'execution.
        if (monMessageReboot.length() == 0) { // Verification d'une condition logique.
          maPensee = texte; // Affectation de valeur a une variable ou modification d'etat.
        }
      }
      // Les traces des autres ESPs (peers) sont délibérément ignorées ici pour // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      // nettoyer au rallumage ! // Instruction d'execution.
    }
  }
  f.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
}

void envoyerCV(String targetMac, String requesterMac) { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  String cvText = ""; // Affectation de valeur a une variable ou modification d'etat.
  if (LittleFS.exists("/profile.json")) { // Verification d'une condition logique.
    File file = LittleFS.open("/profile.json", "r"); // Gestion des fichiers internes de stockage LittleFS.
    cvText = file.readString(); // Affectation de valeur a une variable ou modification d'etat.
    file.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  }
  if (cvText.length() == 0 || !cvText.startsWith("{")) { // Verification d'une condition logique.
    cvText = "{}"; // Affectation de valeur a une variable ou modification d'etat.
  }

  std::vector<String> morceaux; // Tableau dynamique C++ stockant les donnees du programme.
  String texteRestant = cvText; // Affectation de valeur a une variable ou modification d'etat.
  while (texteRestant.length() > 0) { // Execution d'une boucle iterative.
    if (texteRestant.length() <= MAX_PAYLOAD_SIZE) { // Verification d'une condition logique.
      morceaux.push_back(texteRestant); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      break; // Instruction d'execution.
    }
    int splitPos = MAX_PAYLOAD_SIZE; // Affectation de valeur a une variable ou modification d'etat.
    for (int i = MAX_PAYLOAD_SIZE; i > MAX_PAYLOAD_SIZE - 40; i--) { // Execution d'une boucle iterative.
      char c = texteRestant[i]; // Affectation de valeur a une variable ou modification d'etat.
      if (c == ' ' || c == '.' || c == ',' || c == '!') { // Verification d'une condition logique.
        splitPos = i + 1; // Affectation de valeur a une variable ou modification d'etat.
        break; // Instruction d'execution.
      }
    }
    morceaux.push_back(texteRestant.substring(0, splitPos)); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    texteRestant = texteRestant.substring(splitPos); // Affectation de valeur a une variable ou modification d'etat.
  }

  int total = morceaux.size(); // Affectation de valeur a une variable ou modification d'etat.
  if (total > MAX_CHUNKS) // Verification d'une condition logique.
    total = MAX_CHUNKS; // Affectation de valeur a une variable ou modification d'etat.
  uint32_t msgId = millis(); // Affectation de valeur a une variable ou modification d'etat.

  for (int i = 0; i < total; i++) { // Execution d'une boucle iterative.
    String paquet = "C|" + requesterMac + "|" + monID + "|" + String(msgId) + // Affectation de valeur a une variable ou modification d'etat.
                    "|" + String(total) + "|" + String(i) + "|" + morceaux[i]; // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    esp_now_send(broadcastAddress, (uint8_t *)paquet.c_str(), paquet.length()); // Transmission sans fil d'un paquet de donnees via le protocole ESP-NOW.
    delay(45); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  }
}

// ===== LE DÉCOUPEUR INTELLIGENT (Envoi ESP-NOW) ===== // Affectation de valeur a une variable ou modification d'etat.
void envoyerTexteLong() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  String texteRestant = maPensee; // Affectation de valeur a une variable ou modification d'etat.
  std::vector<String> morceaux; // Tableau dynamique C++ stockant les donnees du programme.

  if (texteRestant.length() == 0) { // Verification d'une condition logique.
    morceaux.push_back(""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  } else { // Instruction d'execution.
    while (texteRestant.length() > 0) { // Execution d'une boucle iterative.
      if (texteRestant.length() <= MAX_PAYLOAD_SIZE) { // Verification d'une condition logique.
        morceaux.push_back(texteRestant); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        break; // Instruction d'execution.
      }
      int splitPos = MAX_PAYLOAD_SIZE; // Affectation de valeur a une variable ou modification d'etat.
      for (int i = MAX_PAYLOAD_SIZE; i > MAX_PAYLOAD_SIZE - 40; i--) { // Execution d'une boucle iterative.
        char c = texteRestant[i]; // Affectation de valeur a une variable ou modification d'etat.
        if (c == ' ' || c == '.' || c == ',' || c == '!') { // Verification d'une condition logique.
          splitPos = i + 1; // Affectation de valeur a une variable ou modification d'etat.
          break; // Instruction d'execution.
        }
      }
      morceaux.push_back(texteRestant.substring(0, splitPos)); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      texteRestant = texteRestant.substring(splitPos); // Affectation de valeur a une variable ou modification d'etat.
    }
  }

  int total = morceaux.size(); // Affectation de valeur a une variable ou modification d'etat.
  if (total > MAX_CHUNKS) // Verification d'une condition logique.
    total = MAX_CHUNKS; // Affectation de valeur a une variable ou modification d'etat.
  monMsgID = millis(); // Affectation de valeur a une variable ou modification d'etat.

  String prefix = (monContraste == 1) ? "P1|" : "P0|"; // Affectation de valeur a une variable ou modification d'etat.

  for (int i = 0; i < total; i++) { // Execution d'une boucle iterative.
    String paquet = prefix + monID + "|" + monNomReseau + // Affectation de valeur a une variable ou modification d'etat.
                    "::" + monNomUtilisateur + "|" + String(monMsgID) + "|" + // Appel d'une fonction interne ou d'une memoire de bibliotheque.
                    String(total) + "|" + String(i) + "|" + morceaux[i]; // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    esp_now_send(broadcastAddress, (uint8_t *)paquet.c_str(), paquet.length()); // Transmission sans fil d'un paquet de donnees via le protocole ESP-NOW.
    delay(75); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  }
}

// ===== LE RÉASSEMBLEUR (Réception ESP-NOW) ===== // Affectation de valeur a une variable ou modification d'etat.
void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (fonctionnementMode == 1) { // Verification d'une condition logique.
    return; // Instruction d'execution.
  }
  String senderMac = ""; // Affectation de valeur a une variable ou modification d'etat.
  if (info && info->src_addr) { // Verification d'une condition logique.
    char mac[13]; // Instruction d'execution.
    snprintf(mac, sizeof(mac), "%02x%02x%02x%02x%02x%02x", // Appel d'une fonction interne ou d'une memoire de bibliotheque.
             info->src_addr[0], info->src_addr[1], info->src_addr[2], // Instruction d'execution.
             info->src_addr[3], info->src_addr[4], info->src_addr[5]); // Instruction d'execution.
    senderMac = String(mac); // Affectation de valeur a une variable ou modification d'etat.
    senderMac.toLowerCase(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  }
  if (estBanni(senderMac)) { // Verification d'une condition logique.
    return; // Instruction d'execution.
  }
  char buffer[len + 1]; // Instruction d'execution.
  memcpy(buffer, data, len); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  buffer[len] = '\0'; // Affectation de valeur a une variable ou modification d'etat.
  String msg = String(buffer); // Affectation de valeur a une variable ou modification d'etat.

  // Intercepter les requêtes de CV // Instruction d'execution.
  if (msg.startsWith("R|")) { // Verification d'une condition logique.
    int s1 = msg.indexOf('|', 2); // Affectation de valeur a une variable ou modification d'etat.
    if (s1 > 0) { // Verification d'une condition logique.
      String targetMac = msg.substring(2, s1); // Affectation de valeur a une variable ou modification d'etat.
      String requesterMac = msg.substring(s1 + 1); // Affectation de valeur a une variable ou modification d'etat.
      targetMac.toLowerCase(); targetMac.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      requesterMac.toLowerCase(); requesterMac.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      if (targetMac == monID) { // Verification d'une condition logique.
        cvRequestsQueue.push_back({targetMac, requesterMac}); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      }
    }
    return; // Instruction d'execution.
  }

  // Intercepter les morceaux de CV // Instruction d'execution.
  if (msg.startsWith("C|")) { // Verification d'une condition logique.
    int s1 = msg.indexOf('|', 2); // Affectation de valeur a une variable ou modification d'etat.
    int s2 = msg.indexOf('|', s1 + 1); // Affectation de valeur a une variable ou modification d'etat.
    int s3 = msg.indexOf('|', s2 + 1); // Affectation de valeur a une variable ou modification d'etat.
    int s4 = msg.indexOf('|', s3 + 1); // Affectation de valeur a une variable ou modification d'etat.
    int s5 = msg.indexOf('|', s4 + 1); // Affectation de valeur a une variable ou modification d'etat.
    if (s5 > 0) { // Verification d'une condition logique.
      String requesterMac = msg.substring(2, s1); // Affectation de valeur a une variable ou modification d'etat.
      String sourceMac = msg.substring(s1 + 1, s2); // Affectation de valeur a une variable ou modification d'etat.
      String msgId = msg.substring(s2 + 1, s3); // Affectation de valeur a une variable ou modification d'etat.
      int total = msg.substring(s3 + 1, s4).toInt(); // Affectation de valeur a une variable ou modification d'etat.
      int index = msg.substring(s4 + 1, s5).toInt(); // Affectation de valeur a une variable ou modification d'etat.
      String chunk = msg.substring(s5 + 1); // Affectation de valeur a une variable ou modification d'etat.

      requesterMac.toLowerCase(); requesterMac.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      sourceMac.toLowerCase(); sourceMac.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      if (requesterMac == monID) { // Verification d'une condition logique.
        String cleUnique = "CV_" + sourceMac + "_" + msgId; // Affectation de valeur a une variable ou modification d'etat.
        if (cvsEnAttente.find(cleUnique) == cvsEnAttente.end()) { // Verification d'une condition logique.
          cvsEnAttente[cleUnique] = {"", total, 0, 0, {""}, millis()}; // Affectation de valeur a une variable ou modification d'etat.
        }
        if (cvsEnAttente[cleUnique].morceaux[index] == "") { // Verification d'une condition logique.
          cvsEnAttente[cleUnique].morceaux[index] = chunk; // Affectation de valeur a une variable ou modification d'etat.
          cvsEnAttente[cleUnique].recu++; // Instruction d'execution.
          cvsEnAttente[cleUnique].dernierUpdate = millis(); // Affectation de valeur a une variable ou modification d'etat.
        }
        if (cvsEnAttente[cleUnique].recu == total) { // Verification d'une condition logique.
          String cvComplet = ""; // Affectation de valeur a une variable ou modification d'etat.
          for (int i = 0; i < total; i++) { // Execution d'une boucle iterative.
            cvComplet += cvsEnAttente[cleUnique].morceaux[i]; // Affectation de valeur a une variable ou modification d'etat.
          }
          cvsStockes[sourceMac] = cvComplet; // Affectation de valeur a une variable ou modification d'etat.
          cvsEnAttente.erase(cleUnique); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        }
      }
      return; // Instruction d'execution.
    }
  }
  // 1. Intercepter les paquets de vote // Instruction d'execution.
  if (msg.startsWith("V|")) { // Verification d'une condition logique.
    int s1 = msg.indexOf('|', 2); // Affectation de valeur a une variable ou modification d'etat.
    int s2 = msg.indexOf('|', s1 + 1); // Affectation de valeur a une variable ou modification d'etat.
    if (s1 > 0 && s2 > 0) { // Verification d'une condition logique.
      String cible = msg.substring(2, s1); // Affectation de valeur a une variable ou modification d'etat.
      String voteur = msg.substring(s1 + 1, s2); // Affectation de valeur a une variable ou modification d'etat.
      String emoticon = msg.substring(s2 + 1); // Affectation de valeur a une variable ou modification d'etat.

      votesMap[cible][emoticon]++; // Instruction d'execution.

      if (cible == monID) { // Verification d'une condition logique.
        voteNotifications.push_back({voteur, emoticon, millis()}); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        if (voteNotifications.size() > 5) { // Verification d'une condition logique.
          voteNotifications.erase(voteNotifications.begin()); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        }
      }
    }
    return; // Instruction d'execution.
  }

  // 2. Traitement des graffitis // Instruction d'execution.
  if (msg.startsWith("GF|")) { // Verification d'une condition logique.
    int s1 = msg.indexOf('|', 3); // Affectation de valeur a une variable ou modification d'etat.
    int s2 = msg.indexOf('|', s1 + 1); // Affectation de valeur a une variable ou modification d'etat.
    if (s1 > 0 && s2 > 0) { // Verification d'une condition logique.
      String cibleID = msg.substring(3, s1); // Affectation de valeur a une variable ou modification d'etat.
      String auteur = msg.substring(s1 + 1, s2); // Affectation de valeur a une variable ou modification d'etat.
      String texte = msg.substring(s2 + 1); // Affectation de valeur a une variable ou modification d'etat.

      auteur.replace("|||", " "); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      texte.replace("|||", " "); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      texte.replace("\n", " "); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

      if (cibleID == monID) { // Verification d'une condition logique.
        String displayAuteur = auteur; // Affectation de valeur a une variable ou modification d'etat.
        int sepIdx = auteur.indexOf("::"); // Affectation de valeur a une variable ou modification d'etat.
        if (sepIdx > 0) { // Verification d'une condition logique.
          displayAuteur = auteur.substring(sepIdx + 2); // Nom d'utilisateur // Affectation de valeur a une variable ou modification d'etat.
        }

        String macStr = ""; // Affectation de valeur a une variable ou modification d'etat.
        if (info && info->src_addr) { // Verification d'une condition logique.
          char mac[13]; // Instruction d'execution.
          snprintf(mac, sizeof(mac), "%02x%02x%02x%02x%02x%02x", // Appel d'une fonction interne ou d'une memoire de bibliotheque.
                   info->src_addr[0], info->src_addr[1], info->src_addr[2], // Instruction d'execution.
                   info->src_addr[3], info->src_addr[4], info->src_addr[5]); // Instruction d'execution.
          macStr = String(mac); // Affectation de valeur a une variable ou modification d'etat.
        }

        String storedAuthor = displayAuteur; // Affectation de valeur a une variable ou modification d'etat.
        if (macStr.length() > 0) { // Verification d'une condition logique.
          storedAuthor = macStr + "::" + displayAuteur; // Affectation de valeur a une variable ou modification d'etat.
        }

        String color = "#10b981"; // Fallback, géré par le nouveau mur // Affectation de valeur a une variable ou modification d'etat.
        String newLine = // Affectation de valeur a une variable ou modification d'etat.
            storedAuthor + "|||" + texte + "|||0|||0|||" + color + "|||0|||1.5"; // Instruction d'execution.

        String lines[MAX_GRAFFITIS]; // Instruction d'execution.
        int count = 0; // Affectation de valeur a une variable ou modification d'etat.
        File file = LittleFS.open(GRAFFITIS_FILE, "r"); // Gestion des fichiers internes de stockage LittleFS.
        if (file) { // Verification d'une condition logique.
          while (file.available()) { // Execution d'une boucle iterative.
            String line = file.readStringUntil('\n'); // Affectation de valeur a une variable ou modification d'etat.
            line.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
            if (line.length() > 0) { // Verification d'une condition logique.
              if (count < MAX_GRAFFITIS) // Verification d'une condition logique.
                lines[count++] = line; // Affectation de valeur a une variable ou modification d'etat.
              else { // Alternative de branchement logique.
                for (int i = 0; i < MAX_GRAFFITIS - 1; i++) // Execution d'une boucle iterative.
                  lines[i] = lines[i + 1]; // Affectation de valeur a une variable ou modification d'etat.
                lines[MAX_GRAFFITIS - 1] = line; // Affectation de valeur a une variable ou modification d'etat.
              }
            }
          }
          file.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        }

        if (count < MAX_GRAFFITIS) // Verification d'une condition logique.
          lines[count++] = newLine; // Affectation de valeur a une variable ou modification d'etat.
        else { // Alternative de branchement logique.
          for (int i = 0; i < MAX_GRAFFITIS - 1; i++) // Execution d'une boucle iterative.
            lines[i] = lines[i + 1]; // Affectation de valeur a une variable ou modification d'etat.
          lines[MAX_GRAFFITIS - 1] = newLine; // Affectation de valeur a une variable ou modification d'etat.
        }

        File outFile = LittleFS.open(GRAFFITIS_FILE, "w"); // Gestion des fichiers internes de stockage LittleFS.
        for (int i = 0; i < count; i++) // Execution d'une boucle iterative.
          outFile.println(lines[i]); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        outFile.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      }
    }
    return; // Instruction d'execution.
  }

  // 3. Traitement des pensées (Téléprompteur) // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  int contrast = 0; // Affectation de valeur a une variable ou modification d'etat.
  if (msg.startsWith("P0|")) { // Verification d'une condition logique.
    contrast = 0; // Affectation de valeur a une variable ou modification d'etat.
    msg = msg.substring(3); // Affectation de valeur a une variable ou modification d'etat.
  } else if (msg.startsWith("P1|")) { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    contrast = 1; // Affectation de valeur a une variable ou modification d'etat.
    msg = msg.substring(3); // Affectation de valeur a une variable ou modification d'etat.
  } else if (msg.startsWith("P|")) { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    contrast = 0; // Affectation de valeur a une variable ou modification d'etat.
    msg = msg.substring(2); // Affectation de valeur a une variable ou modification d'etat.
  } else { // Instruction d'execution.
    return; // Paquet inconnu // Instruction d'execution.
  }

  int s[5]; // Instruction d'execution.
  s[0] = msg.indexOf('|'); // Affectation de valeur a une variable ou modification d'etat.
  for (int i = 1; i < 5; i++) // Execution d'une boucle iterative.
    s[i] = msg.indexOf('|', s[i - 1] + 1); // Affectation de valeur a une variable ou modification d'etat.

  if (s[4] > 0) { // Verification d'une condition logique.
    String auteur = msg.substring(0, s[0]); // Affectation de valeur a une variable ou modification d'etat.
    String nom = msg.substring(s[0] + 1, s[1]); // Affectation de valeur a une variable ou modification d'etat.
    String msgId = msg.substring(s[1] + 1, s[2]); // Affectation de valeur a une variable ou modification d'etat.
    int total = msg.substring(s[2] + 1, s[3]).toInt(); // Affectation de valeur a une variable ou modification d'etat.
    int index = msg.substring(s[3] + 1, s[4]).toInt(); // Affectation de valeur a une variable ou modification d'etat.
    String texteChunk = msg.substring(s[4] + 1); // Affectation de valeur a une variable ou modification d'etat.
    int rssi = info->rx_ctrl->rssi; // Affectation de valeur a une variable ou modification d'etat.
    String cleUnique = auteur + "_" + msgId; // Affectation de valeur a une variable ou modification d'etat.

    if (index >= MAX_CHUNKS || index < 0) // Verification d'une condition logique.
      return; // Instruction d'execution.

    if (messagesEnAttente.find(cleUnique) == messagesEnAttente.end()) { // Verification d'une condition logique.
      messagesEnAttente[cleUnique] = {nom, total, 0, rssi, {""}, millis(), contrast}; // Affectation de valeur a une variable ou modification d'etat.
    }

    if (messagesEnAttente[cleUnique].morceaux[index] == "") { // Verification d'une condition logique.
      messagesEnAttente[cleUnique].morceaux[index] = texteChunk; // Affectation de valeur a une variable ou modification d'etat.
      messagesEnAttente[cleUnique].recu++; // Instruction d'execution.
      messagesEnAttente[cleUnique].rssi = rssi; // Affectation de valeur a une variable ou modification d'etat.
    }

    if (messagesEnAttente[cleUnique].recu == total) { // Verification d'une condition logique.
      String texteComplet = ""; // Affectation de valeur a une variable ou modification d'etat.
      for (int i = 0; i < total; i++) // Execution d'une boucle iterative.
        texteComplet += messagesEnAttente[cleUnique].morceaux[i]; // Affectation de valeur a une variable ou modification d'etat.

      bool found = false; // Affectation de valeur a une variable ou modification d'etat.
      for (auto &m : messages) { // Execution d'une boucle iterative.
        if (m.auteur == auteur) { // Verification d'une condition logique.
          m.nomReseau = nom; // Affectation de valeur a une variable ou modification d'etat.
          m.texte = texteComplet; // Affectation de valeur a une variable ou modification d'etat.
          m.rssi = rssi; // Affectation de valeur a une variable ou modification d'etat.
          m.dernierContact = millis(); // Affectation de valeur a une variable ou modification d'etat.
          m.contrast = messagesEnAttente[cleUnique].contrast; // Affectation de valeur a une variable ou modification d'etat.
          found = true; // Affectation de valeur a une variable ou modification d'etat.
          break; // Instruction d'execution.
        }
      }
      if (!found) { // Verification d'une condition logique.
        messages.push_back({auteur, nom, texteComplet, rssi, millis(), messagesEnAttente[cleUnique].contrast}); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        if (messages.size() > MAX_MESSAGES) // Verification d'une condition logique.
          messages.erase(messages.begin()); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      }

      // Extraire le SSID et le nom d'utilisateur // Instruction d'execution.
      String ssidPart = nom; // Affectation de valeur a une variable ou modification d'etat.
      String userPart = nom; // Affectation de valeur a une variable ou modification d'etat.
      int sepIdx = nom.indexOf("::"); // Affectation de valeur a une variable ou modification d'etat.
      if (sepIdx > 0) { // Verification d'une condition logique.
        ssidPart = nom.substring(0, sepIdx); // Affectation de valeur a une variable ou modification d'etat.
        userPart = nom.substring(sepIdx + 2); // Affectation de valeur a une variable ou modification d'etat.
      }

      // Enregistrer le message reçu dans le journal réseau // Instruction d'execution.
      ajouterAuJournal(userPart, ssidPart, texteComplet); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

      messagesEnAttente.erase(cleUnique); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      resauvegarder = true; // Affectation de valeur a une variable ou modification d'etat.
    }
  }
}

// ===== SERVEUR WEB : GESTION DES FICHIERS STATIQUES (MUR) ===== // Affectation de valeur a une variable ou modification d'etat.
bool handleStaticFile(String path, bool forceDownload = false) { // Affectation de valeur a une variable ou modification d'etat.
  if (LittleFS.exists(path)) { // Verification d'une condition logique.
    String contentType = "text/plain"; // Affectation de valeur a une variable ou modification d'etat.
    if (path.endsWith(".html")) // Verification d'une condition logique.
      contentType = "text/html"; // Affectation de valeur a une variable ou modification d'etat.
    else if (path.endsWith(".css")) // Verification d'une condition logique.
      contentType = "text/css"; // Affectation de valeur a une variable ou modification d'etat.
    else if (path.endsWith(".js")) // Verification d'une condition logique.
      contentType = "application/javascript"; // Affectation de valeur a une variable ou modification d'etat.
    else if (path.endsWith(".pdf")) // Verification d'une condition logique.
      contentType = "application/pdf"; // Affectation de valeur a une variable ou modification d'etat.
    else if (path.endsWith(".png")) // Verification d'une condition logique.
      contentType = "image/png"; // Affectation de valeur a une variable ou modification d'etat.
    else if (path.endsWith(".jpg")) // Verification d'une condition logique.
      contentType = "image/jpeg"; // Affectation de valeur a une variable ou modification d'etat.
    else if (path.endsWith(".zip")) // Verification d'une condition logique.
      contentType = "application/zip"; // Affectation de valeur a une variable ou modification d'etat.
    else if (path.endsWith(".ino")) // Verification d'une condition logique.
      contentType = "text/plain"; // Affectation de valeur a une variable ou modification d'etat.

    if (forceDownload) { // Verification d'une condition logique.
      int lastSlash = path.lastIndexOf('/'); // Affectation de valeur a une variable ou modification d'etat.
      String filename = (lastSlash >= 0) ? path.substring(lastSlash + 1) : path; // Affectation de valeur a une variable ou modification d'etat.
      server.sendHeader("Content-Disposition", // Envoi de la reponse HTTP au navigateur du visiteur connecte.
                        "attachment; filename=\"" + filename + "\""); // Affectation de valeur a une variable ou modification d'etat.
    }

    File file = LittleFS.open(path, "r"); // Gestion des fichiers internes de stockage LittleFS.
    server.streamFile(file, contentType); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    file.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    return true; // Instruction d'execution.
  }
  return false; // Instruction d'execution.
}

// ===== API DU MUR ===== // Affectation de valeur a une variable ou modification d'etat.
void handleGetGraffitis() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  File file = LittleFS.open(GRAFFITIS_FILE, "r"); // Gestion des fichiers internes de stockage LittleFS.
  if (!file) { // Verification d'une condition logique.
    server.send(500, "application/json", "[]"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    return; // Instruction d'execution.
  }

  String json = "["; // Affectation de valeur a une variable ou modification d'etat.
  bool first = true; // Affectation de valeur a une variable ou modification d'etat.
  int index = 0; // Affectation de valeur a une variable ou modification d'etat.
  while (file.available()) { // Execution d'une boucle iterative.
    String line = file.readStringUntil('\n'); // Affectation de valeur a une variable ou modification d'etat.
    line.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    if (line.length() > 0) { // Verification d'une condition logique.
      String pieces[7]; // Instruction d'execution.
      int count = 0; // Affectation de valeur a une variable ou modification d'etat.
      int startIdx = 0; // Affectation de valeur a une variable ou modification d'etat.
      while (startIdx < line.length() && count < 7) { // Execution d'une boucle iterative.
        int nextIdx = line.indexOf("|||", startIdx); // Affectation de valeur a une variable ou modification d'etat.
        if (nextIdx == -1) { // Verification d'une condition logique.
          pieces[count++] = line.substring(startIdx); // Affectation de valeur a une variable ou modification d'etat.
          break; // Instruction d'execution.
        }
        pieces[count++] = line.substring(startIdx, nextIdx); // Affectation de valeur a une variable ou modification d'etat.
        startIdx = nextIdx + 3; // Affectation de valeur a une variable ou modification d'etat.
      }

      if (count >= 2) { // Verification d'une condition logique.
        if (!first) // Verification d'une condition logique.
          json += ","; // Affectation de valeur a une variable ou modification d'etat.
        String author = pieces[0]; // Affectation de valeur a une variable ou modification d'etat.
        String msg = pieces[1]; // Affectation de valeur a une variable ou modification d'etat.
        author.replace("\"", "\\\""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        msg.replace("\"", "\\\""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

        json += "{\"index\":" + String(index) + ", \"author\":\"" + author + // Affectation de valeur a une variable ou modification d'etat.
                "\", \"text\":\"" + msg + "\""; // Instruction d'execution.
        if (count >= 7) { // Verification d'une condition logique.
          json += ", \"x\":\"" + pieces[2] + "\", \"y\":\"" + pieces[3] + // Affectation de valeur a une variable ou modification d'etat.
                  "\", \"color\":\"" + pieces[4] + "\", \"rot\":\"" + // Instruction d'execution.
                  pieces[5] + "\", \"size\":\"" + pieces[6] + "\""; // Instruction d'execution.
        }
        json += "}"; // Affectation de valeur a une variable ou modification d'etat.
        first = false; // Affectation de valeur a une variable ou modification d'etat.
        index++; // Instruction d'execution.
      }
    }
  }
  file.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  json += "]"; // Affectation de valeur a une variable ou modification d'etat.

  server.send(200, "application/json", json); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
}

void handlePostGraffiti() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (!server.hasArg("message")) { // Verification d'une condition logique.
    server.send(400, "text/plain", "Requête invalide"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    return; // Instruction d'execution.
  }

  // L'auteur est le nom d'utilisateur configuré pour cet ESP // Instruction d'execution.
  String author = monNomUtilisateur; // Affectation de valeur a une variable ou modification d'etat.
  String message = server.arg("message"); // Affectation de valeur a une variable ou modification d'etat.
  String x = server.hasArg("x") ? server.arg("x") : "0"; // Affectation de valeur a une variable ou modification d'etat.
  String y = server.hasArg("y") ? server.arg("y") : "0"; // Affectation de valeur a une variable ou modification d'etat.
  String color = server.hasArg("color") ? server.arg("color") : ""; // Affectation de valeur a une variable ou modification d'etat.
  String rot = server.hasArg("rot") ? server.arg("rot") : ""; // Affectation de valeur a une variable ou modification d'etat.
  String sz = server.hasArg("size") ? server.arg("size") : ""; // Affectation de valeur a une variable ou modification d'etat.

  auto cleanStr = [](String &s) { // Affectation de valeur a une variable ou modification d'etat.
    s.replace("\n", " "); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    s.replace("|||", " "); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  }; // Instruction d'execution.
  cleanStr(author); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  cleanStr(message); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  cleanStr(x); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  cleanStr(y); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  cleanStr(color); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  cleanStr(rot); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  cleanStr(sz); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

  if (message.length() == 0) { // Verification d'une condition logique.
    server.send(400, "text/plain", "Texte vide interdit"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    return; // Instruction d'execution.
  }

  // Ajouter l'heure si synchronisée // Instruction d'execution.
  String timeStr = obtenirTempsFormate(); // Affectation de valeur a une variable ou modification d'etat.
  if (timeStr.length() > 0) { // Verification d'une condition logique.
    message += " (" + timeStr.substring(1, timeStr.length() - 1) + ")"; // Affectation de valeur a une variable ou modification d'etat.
  }

  String lines[MAX_GRAFFITIS]; // Instruction d'execution.
  int count = 0; // Affectation de valeur a une variable ou modification d'etat.

  File file = LittleFS.open(GRAFFITIS_FILE, "r"); // Gestion des fichiers internes de stockage LittleFS.
  if (file) { // Verification d'une condition logique.
    while (file.available()) { // Execution d'une boucle iterative.
      String line = file.readStringUntil('\n'); // Affectation de valeur a une variable ou modification d'etat.
      line.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      if (line.length() > 0) { // Verification d'une condition logique.
        if (count < MAX_GRAFFITIS) { // Verification d'une condition logique.
          lines[count++] = line; // Affectation de valeur a une variable ou modification d'etat.
        } else { // Instruction d'execution.
          for (int i = 0; i < MAX_GRAFFITIS - 1; i++) // Execution d'une boucle iterative.
            lines[i] = lines[i + 1]; // Affectation de valeur a une variable ou modification d'etat.
          lines[MAX_GRAFFITIS - 1] = line; // Affectation de valeur a une variable ou modification d'etat.
        }
      }
    }
    file.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  }

  String newLine = author + "|||" + message + "|||" + x + "|||" + y + "|||" + // Affectation de valeur a une variable ou modification d'etat.
                   color + "|||" + rot + "|||" + sz; // Instruction d'execution.

  if (count < MAX_GRAFFITIS) { // Verification d'une condition logique.
    lines[count++] = newLine; // Affectation de valeur a une variable ou modification d'etat.
  } else { // Instruction d'execution.
    for (int i = 0; i < MAX_GRAFFITIS - 1; i++) // Execution d'une boucle iterative.
      lines[i] = lines[i + 1]; // Affectation de valeur a une variable ou modification d'etat.
    lines[MAX_GRAFFITIS - 1] = newLine; // Affectation de valeur a une variable ou modification d'etat.
  }

  File outFile = LittleFS.open(GRAFFITIS_FILE, "w"); // Gestion des fichiers internes de stockage LittleFS.
  for (int i = 0; i < count; i++) // Execution d'une boucle iterative.
    outFile.println(lines[i]); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  outFile.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

  server.send(200, "text/plain", "Message enregistré"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
}

void handleRemoteGraffiti() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (!server.hasArg("cible") || !server.hasArg("message")) { // Verification d'une condition logique.
    server.send(400, "text/plain", "Requête invalide"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    return; // Instruction d'execution.
  }
  String cible = server.arg("cible"); // Affectation de valeur a une variable ou modification d'etat.
  String message = server.arg("message"); // Affectation de valeur a une variable ou modification d'etat.
  message.replace("\n", " "); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  message.replace("|||", " "); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (message.length() > 150) // Verification d'une condition logique.
    message = message.substring(0, 150); // Affectation de valeur a une variable ou modification d'etat.

  // Ajouter l'heure si synchronisée // Instruction d'execution.
  String timeStr = obtenirTempsFormate(); // Affectation de valeur a une variable ou modification d'etat.
  if (timeStr.length() > 0) { // Verification d'une condition logique.
    message += " (" + timeStr.substring(1, timeStr.length() - 1) + ")"; // Affectation de valeur a une variable ou modification d'etat.
  }

  // Format packet: GF|cible|auteurReseau::auteurUser|message // Instruction d'execution.
  String paquet = "GF|" + cible + "|" + monNomReseau + // Affectation de valeur a une variable ou modification d'etat.
                  "::" + monNomUtilisateur + "|" + message; // Instruction d'execution.
  esp_now_send(broadcastAddress, (uint8_t *)paquet.c_str(), paquet.length()); // Transmission sans fil d'un paquet de donnees via le protocole ESP-NOW.

  server.send(200, "text/plain", "Message envoyé"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
}

// Supprime un graffiti spécifique du fichier messages.txt en se basant sur son // Instruction d'execution.
// index de ligne. Reçoit le paramètre POST 'index' indiquant la ligne à ignorer // Instruction d'execution.
// lors de la réécriture du fichier. // Instruction d'execution.
void handleDeleteGraffiti() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (!server.hasArg("index")) { // Verification d'une condition logique.
    server.send(400, "text/plain", "Index manquant"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    return; // Instruction d'execution.
  }
  int targetIndex = server.arg("index").toInt(); // Affectation de valeur a une variable ou modification d'etat.

  // Lecture de tous les graffitis existants // Instruction d'execution.
  File file = LittleFS.open(GRAFFITIS_FILE, "r"); // Gestion des fichiers internes de stockage LittleFS.
  if (!file) { // Verification d'une condition logique.
    server.send(500, "text/plain", "Impossible d'ouvrir le fichier"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    return; // Instruction d'execution.
  }

  std::vector<String> lines; // Tableau dynamique C++ stockant les donnees du programme.
  while (file.available()) { // Execution d'une boucle iterative.
    String line = file.readStringUntil('\n'); // Affectation de valeur a une variable ou modification d'etat.
    line.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    if (line.length() > 0) { // Verification d'une condition logique.
      lines.push_back(line); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    }
  }
  file.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

  // Validation de l'index de suppression ciblé // Instruction d'execution.
  if (targetIndex < 0 || targetIndex >= (int)lines.size()) { // Verification d'une condition logique.
    server.send(400, "text/plain", "Index invalide"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    return; // Instruction d'execution.
  }

  // Réécriture de messages.txt en excluant la ligne de l'index supprimé // Instruction d'execution.
  File outFile = LittleFS.open(GRAFFITIS_FILE, "w"); // Gestion des fichiers internes de stockage LittleFS.
  if (!outFile) { // Verification d'une condition logique.
    server.send(500, "text/plain", "Erreur ecriture"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    return; // Instruction d'execution.
  }

  for (int i = 0; i < (int)lines.size(); i++) { // Execution d'une boucle iterative.
    if (i != targetIndex) { // Verification d'une condition logique.
      outFile.println(lines[i]); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    }
  }
  outFile.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

  server.send(200, "text/plain", "OK"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
}

// ===== CONFIGURATION WIFI ===== // Affectation de valeur a une variable ou modification d'etat.
void handleGetConfig() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  String favJson = "["; // Affectation de valeur a une variable ou modification d'etat.
  for (size_t i = 0; i < favorites.size(); i++) { // Execution d'une boucle iterative.
    if (i > 0) favJson += ","; // Verification d'une condition logique.
    favJson += "\"" + favorites[i] + "\""; // Affectation de valeur a une variable ou modification d'etat.
  }
  favJson += "]"; // Affectation de valeur a une variable ou modification d'etat.

  String banJson = "["; // Affectation de valeur a une variable ou modification d'etat.
  for (size_t i = 0; i < banned.size(); i++) { // Execution d'une boucle iterative.
    if (i > 0) banJson += ","; // Verification d'une condition logique.
    banJson += "\"" + banned[i] + "\""; // Affectation de valeur a une variable ou modification d'etat.
  }
  banJson += "]"; // Affectation de valeur a une variable ou modification d'etat.

  String json = "{\"ssid\":\"" + monNomReseau + "\",\"pwd\":\"" + // Affectation de valeur a une variable ou modification d'etat.
                monMotDePasse + "\",\"rebootMsg\":\"" + monMessageReboot + // Instruction d'execution.
                "\",\"username\":\"" + monNomUtilisateur + "\",\"mode\":" + // Instruction d'execution.
                String(fonctionnementMode) + ",\"fsTotal\":" + String(LittleFS.totalBytes()) +  // Appel d'une fonction interne ou d'une memoire de bibliotheque.
                ",\"fsUsed\":" + String(LittleFS.usedBytes()) + ",\"favorites\":" + favJson + // Appel d'une fonction interne ou d'une memoire de bibliotheque.
                ",\"banned\":" + banJson + ",\"contrast\":" + String(monContraste) + "}"; // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  server.send(200, "application/json", json); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
}

void handlePostConfig() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (server.hasArg("ssid") && server.hasArg("pwd")) { // Verification d'une condition logique.
    String newSsid = server.arg("ssid"); // Affectation de valeur a une variable ou modification d'etat.
    String newPwd = server.arg("pwd"); // Affectation de valeur a une variable ou modification d'etat.
    String newRebootMsg = // Affectation de valeur a une variable ou modification d'etat.
        server.hasArg("rebootMsg") ? server.arg("rebootMsg") : ""; // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    String newUsername = // Affectation de valeur a une variable ou modification d'etat.
        server.hasArg("username") ? server.arg("username") : ""; // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    String newMode = // Affectation de valeur a une variable ou modification d'etat.
        server.hasArg("mode") ? server.arg("mode") : String(fonctionnementMode); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

    newSsid.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    newPwd.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    newRebootMsg.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    newUsername.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    newMode.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

    newSsid.replace("\r", ""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    newSsid.replace("\n", ""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    newPwd.replace("\r", ""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    newPwd.replace("\n", ""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    newRebootMsg.replace("\r", " "); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    newRebootMsg.replace("\n", " "); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    newUsername.replace("\r", ""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    newUsername.replace("\n", ""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    newMode.replace("\r", ""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    newMode.replace("\n", ""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

    if (newSsid.length() > 0) { // Verification d'une condition logique.
      File f = LittleFS.open(CONFIG_FILE, "w"); // Gestion des fichiers internes de stockage LittleFS.
      if (f) { // Verification d'une condition logique.
        f.println(newSsid); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        f.println(newPwd); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        f.println(newRebootMsg); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        f.println(newUsername); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        f.println(newMode); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        f.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        server.send(200, "text/plain", "OK"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
        delay(500); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        ESP.restart(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      } else { // Instruction d'execution.
        server.send(500, "text/plain", "Erreur écriture"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
      }
    } else { // Instruction d'execution.
      server.send(400, "text/plain", "SSID invalide"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    }
  } else { // Instruction d'execution.
    server.send(400, "text/plain", "Paramètres manquants"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
  }
}

// ===== INTERFACE WEB (Téléprompteur) ===== // Affectation de valeur a une variable ou modification d'etat.
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover">
  <title>Civvi - Tableau de Bord Cyber-Naturel</title>
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

    /* Control Bar */
    .control-bar {
      background: rgba(20, 19, 16, 0.9);
      backdrop-filter: blur(10px);
      border-top: 1px solid var(--border);
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 10px 90px;
      gap: 20px;
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
    .msg-box .kb-trigger-btn {
      background: none;
      border: none;
      color: var(--primary);
      cursor: pointer;
      font-size: 16px;
      position: absolute;
      right: 12px;
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

    /* Keyboard Overlay */
    .kb-overlay {
      position: fixed;
      bottom: -60%; left: 0;
      width: 100vw; height: 42%;
      background: rgba(20, 19, 16, 0.98);
      backdrop-filter: blur(15px);
      border-top: 1px solid var(--primary);
      z-index: 2200;
      display: flex; flex-direction: column; justify-content: center; align-items: center;
      transition: bottom 0.4s cubic-bezier(0.65, 0, 0.35, 1);
      padding: 15px 0;
    }
    .kb-overlay.actif { bottom: 0; }
    .kb-row { display: flex; gap: 8px; margin-bottom: 8px; }
    .kb-key { 
      background: var(--surface);
      color: var(--text);
      width: 42px; height: 42px; 
      display: flex; justify-content: center; align-items: center; 
      border-radius: 6px; 
      font-size: 16px; font-weight: bold; 
      cursor: pointer;
      user-select: none;
      transition: all 0.1s;
      border: 1px solid var(--border);
    }
    .kb-key:active { background: var(--primary); color: #121212; transform: scale(0.95); }
    .kb-space { width: 280px; }
    .kb-backspace { width: 75px; background: rgba(217, 149, 43, 0.15); color: var(--primary); }
    .kb-close { 
      background: #ef4444; color: white; 
      padding: 8px 30px; border-radius: 6px; 
      font-size: 12px; font-weight: bold; cursor: pointer; margin-top: 10px;
      text-transform: uppercase;
      letter-spacing: 1px;
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
    #ov-lorem { background: #2b1f3d; clip-path: circle(0% at 100% 100%); }
    #ov-lorem.actif { clip-path: circle(150% at 100% 100%); }

    /* Overlays */
    .overlay {
      position: fixed; top: 0; left: 0; width: 100vw; height: 100vh; height: 100dvh;
      z-index: 2000; display: flex; flex-direction: column; justify-content: flex-start; align-items: center;
      color: var(--text); pointer-events: none;
      transition: clip-path 0.8s cubic-bezier(0.65, 0, 0.35, 1);
      padding: 80px 20px 70px 20px;
      box-sizing: border-box;
      overflow-y: auto;
    }
    .overlay.actif { pointer-events: auto; }

    #ov-profil { background: #1c0d0d; clip-path: circle(0% at 0% 0%); } 
    #ov-messagerie { background: #0c121e; clip-path: circle(0% at 100% 0%); } 
    #ov-configuration { background: #1c1c0a; clip-path: circle(0% at 0% 100%); } 
    #ov-actu { background: rgba(15, 14, 12, 0.98); clip-path: circle(0% at 50% 100%); z-index: 2010; } 
    #ov-profil-viewer { background: rgba(20, 19, 16, 0.98); clip-path: circle(0% at 50% 50%); z-index: 2100; display: flex; flex-direction: column; justify-content: center; align-items: center; pointer-events: none; transition: clip-path 0.5s ease-in-out; }
    #ov-profil-viewer.actif { clip-path: circle(150% at 50% 50%); pointer-events: auto; }

    #ov-profil.actif { clip-path: circle(150% at 0% 0%); }
    #ov-messagerie.actif { clip-path: circle(150% at 100% 0%); }
    #ov-configuration.actif { clip-path: circle(150% at 0% 100%); }
    #ov-actu.actif { clip-path: circle(150% at 50% 100%); }

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
      width: 12px;
      height: 12px;
      background-color: #10b981;
      border-radius: 50%;
      pointer-events: none;
      animation: blink 2.5s infinite;
    }
    @keyframes blink {
      0% { opacity: 0.3; }
      50% { opacity: 1; box-shadow: 0 0 8px #10b981; }
      100% { opacity: 0.3; }
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
        display: grid !important;
        grid-template-columns: 1fr auto;
        grid-template-rows: auto auto;
        gap: 10px;
        padding: 10px 90px !important;
      }
      .control-bar .left-controls {
        grid-column: 1 / span 2;
        grid-row: 2;
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
        grid-column: 1;
        grid-row: 1;
        width: 100%;
        min-height: 54px !important;
        margin: 0 !important;
        align-self: stretch !important;
      }
      .control-bar .btn {
        grid-column: 2;
        grid-row: 1;
        height: 100%;
        min-height: 54px;
        align-self: stretch;
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
      font-size: 12px;
      font-style: italic;
      color: var(--secondary);
      background: rgba(186, 207, 136, 0.08);
      border: 1px solid rgba(186, 207, 136, 0.2);
      padding: 6px 16px;
      border-radius: 20px;
      letter-spacing: 0.5px;
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
      opacity: 0;
      visibility: hidden;
      pointer-events: none;
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

<div id="splash-screen">
    <div class="splash-scan-line"></div>
    <div class="splash-container">
        <div class="splash-logo">CIVVI</div>
        <div class="splash-status" id="splash-status-text">Scan du réseau en cours...</div>
        <div class="splash-counter" id="splash-counter-text">Recherche de modules actifs...</div>
    </div>
</div>

<div id="mode-selection-screen" class="hidden">
    <div class="splash-scan-line"></div>
    <div class="mode-container">
        <h1 class="mode-title">CIVVI - Mode Réseau</h1>
        <p class="mode-subtitle">Choisissez le mode de fonctionnement de ce module</p>
        
        <div class="mode-choices">
            <div class="mode-card" id="mode-card-relais" onclick="selectOperatingMode(0)">
                <div class="mode-card-icon">📡</div>
                <div class="mode-card-title">Mode Relais (P2P)</div>
                <div class="mode-card-desc">Réseau décentralisé ESP-NOW. Diffusez votre pensée et captez les messages des autres relais à portée de signal.</div>
                <span class="mode-badge-active" id="badge-active-relais">Actif</span>
            </div>
            
            <div class="mode-card" id="mode-card-salon" onclick="selectOperatingMode(1)">
                <div class="mode-card-icon">💬</div>
                <div class="mode-card-title">Salon de Discussion</div>
                <div class="mode-card-desc">Salon de discussion local centralisé sur cet ESP. Clavardez en temps réel avec toutes les personnes connectées à ce point d'accès.</div>
                
                <div class="form-group" style="margin-top: 15px; width: 100%;" onclick="event.stopPropagation()">
                    <label style="color: var(--primary); font-size: 10px;">Votre Pseudonyme :</label>
                    <input type="text" id="salon-username-input" placeholder="Pseudo de discussion..." maxlength="20" style="padding: 6px; font-size: 12px; margin-top: 4px; background: rgba(0,0,0,0.3); border: 1px solid var(--border); color: var(--text); border-radius: 6px; outline: none; width: 100%;">
                </div>
                
                <span class="mode-badge-active" id="badge-active-salon">Actif</span>
            </div>
        </div>
        
        <div id="mode-selection-status" style="margin-top: 20px; font-size: 12px; color: var(--text-muted); min-height: 20px;"></div>
    </div>
</div>

<div class="esp-interface">
    
    <div class="lignes-fond"></div>



    <div id="btn-profil" class="btn-coin c1" onclick="ouvrirOverlay('ov-profil')" title="profil">
        <svg viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
            <circle cx="12" cy="12" r="6" fill="#ffffff" />
        </svg>
    </div>

    <div id="btn-messagerie" class="btn-coin c3" onclick="ouvrirOverlay('ov-messagerie')" title="messagerie">
        <svg viewBox="0 0 24 24" fill="none" stroke="#1e293b" stroke-width="3" stroke-linecap="round" stroke-linejoin="round">
            <line x1="5" y1="19" x2="19" y2="5" />
        </svg>
    </div>

    <div id="btn-configuration" class="btn-coin c11" onclick="ouvrirOverlay('ov-configuration')" title="configuration">
        <svg viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="2.5" stroke-linejoin="round">
            <polygon points="12,4 4,18 20,18" fill="#ffffff" />
        </svg>
    </div>

    <div id="btn-accueil" class="btn-coin c12" onclick="fermerOverlays(true)" title="accueil">
        <svg viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
            <rect x="5" y="5" width="14" height="14" fill="#ffffff" />
        </svg>
    </div>

    <div class="top-bar">
        <div style="display:flex; justify-content:space-between; align-items:center; width:100%;">
            <div class="header-title" id="header-user">Chargement...</div>
            <div class="top-info">
                <span id="header-ssid" style="color: var(--text-muted); font-size:11px; margin-right:8px; font-style:italic;"></span>
                <span class="badge" id="module-count">0 Module(s)</span>
            </div>
        </div>
        <div class="my-thought-marquee-container">
            <div id="my-thought-marquee" class="my-thought-marquee">Mon dernier message : Chargement...</div>
        </div>
    </div>

    <div class="zone-civvi">
        <div id="notifications-area"></div>
        <div id="prompters">
        </div>
    </div>

    <div class="bottom-bar" onclick="ouvrirOverlay('ov-actu')">
        <div class="marquee-actu" id="marquee-actu-text">Chargement de l'actualité réseau...</div>
    </div>

    <div class="control-bar">
        <div class="left-controls" style="display:flex; flex-direction:column; gap:6px; min-width:130px; justify-content:center; height:100%;">
            <div class="sort-box" style="display:flex; flex-direction:column; gap:2px;">
                <span style="font-size:9px; text-transform:uppercase; color:var(--text-muted); letter-spacing:0.5px;">Tri Voisins :</span>
                <select id="sort-select" onchange="changeSortMode(this.value)" style="background: rgba(0,0,0,0.3); border: 1px solid var(--border); color: var(--text); border-radius: 4px; padding: 2px 4px; font-size: 11px; outline: none; cursor: pointer; font-family: inherit; width: 100%;">
                  <option value="rssi_desc">RSSI Décroissant</option>
                  <option value="rssi_asc">RSSI Croissant</option>
                  <option value="alpha_asc">Nom (A-Z)</option>
                  <option value="alpha_desc">Nom (Z-A)</option>
                </select>
            </div>
            <button onclick="update()" class="btn" style="padding: 4px 8px; font-size: 11px; display: flex; align-items: center; justify-content: center; gap: 4px; border-radius: 4px; cursor: pointer; border: 1px solid var(--border); background: var(--surface-high); color: var(--text); font-family: inherit; width: 100%; box-shadow: 0 2px 5px rgba(0,0,0,0.2);">
              <span>🔄 Actualiser</span>
            </button>
            <div class="speed-box" style="display:flex; flex-direction:column; gap:2px;">
                <span style="font-size:9px; text-transform:uppercase; color:var(--text-muted); letter-spacing:0.5px;">Vitesse: <span id="speed-value">1.0</span>x</span>
                <input type="range" id="speed-slider" min="0.2" max="2" step="0.1" value="1.0" style="width:100%; height:6px; accent-color:var(--primary); cursor:pointer;">
            </div>
        </div>
        <div class="msg-box" style="display:flex; flex-grow:1; flex-direction:column; background:rgba(0,0,0,0.3); border:1px solid var(--border); border-radius:6px; padding:6px 12px; position:relative; min-height:68px; justify-content:center; align-self:center;">
            <textarea id="msg" placeholder="Écrivez votre pensée ici..." maxlength="3500" style="background:none; border:none; color:var(--text); font-size:13px; outline:none; width:100%; height:100%; resize:none; font-family:inherit; padding-right:80px; line-height:1.4;"></textarea>
            <span id="char-counter" style="font-size:10px; color:var(--text-muted); position:absolute; right:40px; bottom:8px;">0 / 3500</span>
            <button class="kb-trigger-btn" onclick="toggleVirtualKeyboard(event)" style="background:none; border:none; color:var(--primary); cursor:pointer; font-size:16px; position:absolute; right:12px; bottom:6px;" title="Clavier virtuel">⌨️</button>
        </div>
        <button class="btn" onclick="sendThought()">Diffuser</button>
    </div>

    <div id="keyboard-overlay" class="kb-overlay">
        <div class="kb-row">
            <div class="kb-key" onclick="taperLettre('A')">A</div>
            <div class="kb-key" onclick="taperLettre('Z')">Z</div>
            <div class="kb-key" onclick="taperLettre('E')">E</div>
            <div class="kb-key" onclick="taperLettre('R')">R</div>
            <div class="kb-key" onclick="taperLettre('T')">T</div>
            <div class="kb-key" onclick="taperLettre('Y')">Y</div>
            <div class="kb-key" onclick="taperLettre('U')">U</div>
            <div class="kb-key" onclick="taperLettre('I')">I</div>
            <div class="kb-key" onclick="taperLettre('O')">O</div>
            <div class="kb-key" onclick="taperLettre('P')">P</div>
        </div>
        <div class="kb-row">
            <div class="kb-key" onclick="taperLettre('Q')">Q</div>
            <div class="kb-key" onclick="taperLettre('S')">S</div>
            <div class="kb-key" onclick="taperLettre('D')">D</div>
            <div class="kb-key" onclick="taperLettre('F')">F</div>
            <div class="kb-key" onclick="taperLettre('G')">G</div>
            <div class="kb-key" onclick="taperLettre('H')">H</div>
            <div class="kb-key" onclick="taperLettre('J')">J</div>
            <div class="kb-key" onclick="taperLettre('K')">K</div>
            <div class="kb-key" onclick="taperLettre('L')">L</div>
            <div class="kb-key" onclick="taperLettre('M')">M</div>
        </div>
        <div class="kb-row">
            <div class="kb-key" onclick="taperLettre('W')">W</div>
            <div class="kb-key" onclick="taperLettre('X')">X</div>
            <div class="kb-key" onclick="taperLettre('C')">C</div>
            <div class="kb-key" onclick="taperLettre('V')">V</div>
            <div class="kb-key" onclick="taperLettre('B')">B</div>
            <div class="kb-key" onclick="taperLettre('N')">N</div>
            <div class="kb-key kb-backspace" onclick="effacerDernierLettre()">Retr</div>
        </div>
        <div class="kb-row">
            <div class="kb-key kb-space" onclick="taperLettre(' ')">Espace</div>
        </div>
        <div class="kb-close" onclick="fermerClavier(event)">Masquer</div>
    </div>

    <div id="ov-profil" class="overlay">
        <div class="content">
            <h1>Profil</h1>
            <div class="line"></div>
            <p class="sub">Personnalisez votre avatar vestimentaire.</p>
            
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
                  <!-- Tête (Cheveux) -->
                  <div class="form-group">
                    <label>👨‍🦱 Cheveux / Tête :</label>
                    <div class="color-swatches-row">
                      <button class="color-swatch-btn" onclick="selectAvatarColor('head', 'transparent')" style="background:rgba(0,0,0,0.3); display:flex; align-items:center; justify-content:center; color:#9ca3af; font-size:10px;" title="Chauve">❌</button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('head', '#ffffff')" style="background:#ffffff;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('head', '#000000')" style="background:#000000;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('head', '#3d2314')" style="background:#3d2314;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('head', '#634439')" style="background:#634439;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('head', '#e6be8a')" style="background:#e6be8a;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('head', '#b55239')" style="background:#b55239;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('head', '#9ca3af')" style="background:#9ca3af;"></button>
                    </div>
                  </div>
                  
                  <!-- Torso (Buste) -->
                  <div class="form-group">
                    <label>👕 Buste (Vêtement) :</label>
                    <div class="color-swatches-row" style="margin-bottom:6px;">
                      <button class="color-swatch-btn" onclick="selectAvatarColor('torso', '#ffffff')" style="background:#ffffff;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('torso', '#ef4444')" style="background:#ef4444;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('torso', '#eab308')" style="background:#eab308;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('torso', '#22c55e')" style="background:#22c55e;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('torso', '#3b82f6')" style="background:#3b82f6;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('torso', '#ec4899')" style="background:#ec4899;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('torso', '#a855f7')" style="background:#a855f7;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('torso', '#f97316')" style="background:#f97316;"></button>
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

                  <!-- Legs (Jambes) -->
                  <div class="form-group">
                    <label>👖 Jambes (Pantalon) :</label>
                    <div class="color-swatches-row" style="margin-bottom:6px;">
                      <button class="color-swatch-btn" onclick="selectAvatarColor('legs', '#ffffff')" style="background:#ffffff;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('legs', '#ef4444')" style="background:#ef4444;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('legs', '#eab308')" style="background:#eab308;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('legs', '#22c55e')" style="background:#22c55e;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('legs', '#3b82f6')" style="background:#3b82f6;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('legs', '#ec4899')" style="background:#ec4899;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('legs', '#a855f7')" style="background:#a855f7;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('legs', '#f97316')" style="background:#f97316;"></button>
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

                  <!-- Feet (Chaussures) -->
                  <div class="form-group">
                    <label>👟 Chaussures :</label>
                    <div class="color-swatches-row">
                      <button class="color-swatch-btn" onclick="selectAvatarColor('feet', '#ffffff')" style="background:#ffffff;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('feet', '#ef4444')" style="background:#ef4444;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('feet', '#eab308')" style="background:#eab308;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('feet', '#22c55e')" style="background:#22c55e;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('feet', '#3b82f6')" style="background:#3b82f6;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('feet', '#ec4899')" style="background:#ec4899;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('feet', '#a855f7')" style="background:#a855f7;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('feet', '#f97316')" style="background:#f97316;"></button>
                      <button class="color-swatch-btn" onclick="selectAvatarColor('feet', '#1f2937')" style="background:#1f2937;"></button>
                    </div>
                  </div>
                </div>
              </div>

              <div style="display:flex; gap:10px; margin-top:20px;">
                <button class="btn" onclick="saveStructuredProfile()" style="flex:1;">Enregistrer le profil</button>
                <button class="btn btn-sec" onclick="resetAvatar()" style="flex:1;">Réinitialiser</button>
              </div>
            </div>
        </div>
    </div>

    <div id="ov-messagerie" class="overlay">
        <div class="content">
            <h1>Messagerie</h1>
            <div class="line"></div>
            <p class="sub">messages reçus</p>
            
            <div id="received-graffitis-list" style="display:flex; flex-direction:column; gap:12px; max-height:400px; overflow-y:auto; width:100%; max-width:500px; padding-right:10px;">
            </div>
        </div>
    </div>

    <div id="ov-configuration" class="overlay">
        <div class="content">
            <h1>Configuration</h1>
            <div class="line"></div>
            <p class="sub">Options Réseau & Configuration</p>
            
            <div class="card">
              <div class="card-title">1. Configuration réseau</div>
              <div class="form-group">
                <label>Nom du réseau (SSID) :</label>
                <input type="text" id="config-ssid" placeholder="Ex: Relais-Civvi">
              </div>
              <div class="form-group">
                <label>Mot de passe (Vide = Réseau ouvert) :</label>
                <input type="password" id="config-pwd" placeholder="Optionnel">
              </div>
              <div class="form-group">
                <label>Nom d'utilisateur affiché :</label>
                <input type="text" id="config-username" placeholder="Votre pseudonyme">
              </div>
              <div class="form-group">
                <label>Message par défaut au rallumage :</label>
                <textarea id="config-reboot-msg" style="height:50px;" placeholder="Message de reboot..."></textarea>
              </div>
            </div>

            <div class="card">
              <div class="card-title">2. Espace Mémoire Flash (LittleFS)</div>
              <div id="fs-space-info" style="font-size:12px; color:var(--text-muted); line-height:1.5;">Chargement de l'espace...</div>
            </div>

            <div class="card">
              <div class="card-title">3. Historique des diffusions</div>
              <div id="broadcast-history-list" style="display:flex; flex-direction:column; gap:8px; max-height:180px; overflow-y:auto; font-size:12px; margin-bottom:12px;">
              </div>
              <div style="display:flex; gap:10px;">
                <button class="btn btn-sec btn-danger" style="flex:1; padding:6px; font-size:11px;" onclick="clearJournal()">Effacer Journal</button>
                <button class="btn btn-sec btn-danger" style="flex:1; padding:6px; font-size:11px;" onclick="clearHistory()">Effacer Hist.</button>
              </div>
            </div>

            <div class="card">
              <div class="card-title">4. Membres Favoris</div>
              <div id="favorites-list-container" style="display:flex; flex-direction:column; gap:6px; max-height:150px; overflow-y:auto; font-size:12px; margin-bottom:10px;">
              </div>
            </div>

            <div class="card">
              <div class="card-title">5. Sauvegarder & Redémarrer</div>
              <button class="btn" style="width:100%;" onclick="saveConfiguration()">Appliquer & Redémarrer</button>
            </div>

            <div class="card" style="text-align:center;">
              <div class="card-title">6. Crédits</div>
              <p style="font-size:11px; color:var(--text-muted); margin-bottom:8px;">Projet Civvi v5 &copy; 2026</p>
              <a href="https://sites.google.com/view/achetertespets" target="_blank" style="color:var(--secondary); font-size:12px; text-decoration:underline;">Visiter le site officiel de Civvi</a>
            </div>
        </div>
    </div>

    <div id="ov-actu" class="overlay" onclick="fermerOverlays()" style="background: rgba(10, 9, 8, 0.98); justify-content: center; align-items: center; overflow: hidden; padding: 0; cursor: pointer;">
        <div id="ov-actu-marquee" style="width: auto; min-width: 100%; white-space: nowrap; font-family: Georgia, serif; font-style: italic; font-size: 4.8vw; color: var(--primary); will-change: transform; display: inline-block; text-shadow: 0 0 25px rgba(217, 149, 43, 0.5);">
            En attente d'actualité...
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

    <div id="ov-lorem" class="overlay" style="background:#2b1f3d;">
        <div class="content">
            <h1>Lorem Ipsum</h1>
            <div class="line"></div>
            <p class="sub">Options d'affichage et de rendu</p>
            
            <div class="card">
              <div class="card-title">Contrastes & Accessibilité</div>
              <p style="font-size:12px; line-height:1.5; color:var(--text-muted); margin-bottom:15px;">
                Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed non risus. Suspendisse lectus tortor, dignissim sit amet, tempor ac, condimentum ac, nisi.
              </p>
              
              <label style="display:flex; align-items:center; gap:10px; cursor:pointer; user-select:none; font-size:13px;">
                <input type="checkbox" id="contrast-checkbox" onchange="toggleContrastMode(this.checked)" style="width:20px; height:20px; cursor:pointer; accent-color:var(--primary);">
                <span>Mode Contraste Élevé (Texte Noir sur Fond Blanc)</span>
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

<script>
    let myFavorites = [];
    let myBanned = [];
    const cvsStockes = {};

    let avatarState = {
      head_color: 'transparent',
      torso_color: '#e5e7eb',
      torso_pattern: '',
      legs_color: '#e5e7eb',
      legs_pattern: '',
      feet_color: '#e5e7eb'
    };

    window.selectAvatarColor = (part, color) => {
      avatarState[`${part}_color`] = color;
      applyAvatarState();
    };

    window.selectAvatarPattern = (part, pattern) => {
      avatarState[`${part}_pattern`] = pattern;
      applyAvatarState();
    };

    window.applyAvatarState = () => {
      // Head
      let headEl = document.getElementById('part-head');
      if (headEl) {
        if (avatarState.head_color === 'transparent') {
          headEl.style.opacity = '0.3';
          headEl.style.backgroundColor = '';
        } else {
          headEl.style.opacity = '1';
          headEl.style.backgroundColor = avatarState.head_color;
        }
      }
      
      // Torso
      let torsoEl = document.getElementById('part-torso');
      if (torsoEl) {
        torsoEl.style.backgroundColor = avatarState.torso_color;
      }
      let torsoPatternEl = document.getElementById('pattern-torso');
      if (torsoPatternEl) {
        torsoPatternEl.className = 'pattern-overlay';
        if (avatarState.torso_pattern) torsoPatternEl.classList.add(avatarState.torso_pattern);
      }

      // Legs
      let legsEl = document.getElementById('part-legs');
      if (legsEl) {
        legsEl.style.backgroundColor = avatarState.legs_color;
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
        footLEl.style.backgroundColor = avatarState.feet_color;
        footREl.style.backgroundColor = avatarState.feet_color;
      }

      // Active border swatches styling
      document.querySelectorAll('.color-swatch-btn').forEach(btn => btn.classList.remove('active'));
      document.querySelectorAll('.pattern-select-btn').forEach(btn => btn.classList.remove('active'));
    };

    window.resetAvatar = () => {
      avatarState = {
        head_color: 'transparent',
        torso_color: '#e5e7eb',
        torso_pattern: '',
        legs_color: '#e5e7eb',
        legs_pattern: '',
        feet_color: '#e5e7eb'
      };
      applyAvatarState();
    };

    window.showModeSelection = () => {
      document.getElementById('mode-selection-screen').classList.remove('hidden');
    };

    window.toggleContrastMode = (checked) => {
      let fd = new URLSearchParams();
      fd.append('contrast', checked ? '1' : '0');
      fetch('/api/my_contrast', { method: 'POST', body: fd })
        .then(() => {
          fetch('/api/trigger_broadcast', { method: 'POST' });
        });
    };

    window.toggleFavoritePeer = (mac, name) => {
      let fd = new URLSearchParams();
      fd.append('mac', mac);
      fetch('/api/favorites/toggle', { method: 'POST', body: fd })
        .then(() => {
          fetch('/api/config')
            .then(r => r.json())
            .then(data => {
              myFavorites = data.favorites || [];
              let viewer = document.getElementById('ov-cv-viewer');
              if (viewer && viewer.classList.contains('actif')) {
                let text = cvsStockes[mac] || "{}";
                document.getElementById('cv-viewer-content').innerHTML = renderStructuredProfile(text, name, mac);
              }
              update();
              loadFavoritesAndBannedUI();
            });
        });
    };

    window.toggleBannedPeer = (mac, name) => {
      let fd = new URLSearchParams();
      fd.append('mac', mac);
      fetch('/api/banned/toggle', { method: 'POST', body: fd })
        .then(() => {
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

    window.loadFavoritesAndBannedUI = () => {
      let favList = document.getElementById('favorites-list-container');
      
      if (favList) {
        favList.innerHTML = "";
        if (myFavorites.length === 0) {
          favList.innerHTML = '<div style="color:var(--text-muted); font-style:italic;">Aucun favori pour le moment.</div>';
        } else {
          myFavorites.forEach(mac => {
            let div = document.createElement('div');
            div.style = "display:flex; justify-content:space-between; align-items:center; background:rgba(255,255,255,0.02); padding:4px 8px; border-radius:4px;";
            div.innerHTML = `
              <span>🌟 <strong style="color:var(--primary);">${mac}</strong> 🌟</span>
              <button class="btn btn-danger" style="padding:2px 6px; font-size:9px;" onclick="toggleFavoritePeer('${mac}', '')">Retirer</button>
            `;
            favList.appendChild(div);
          });
        }
      }
    };

    window.saveStructuredProfile = () => {
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

    window.loadStructuredProfile = () => {
      fetch('/api/profile')
        .then(r => r.json())
        .then(data => {
          avatarState.head_color = data.head_color || 'transparent';
          avatarState.torso_color = data.torso_color || '#e5e7eb';
          avatarState.torso_pattern = data.torso_pattern || '';
          avatarState.legs_color = data.legs_color || '#e5e7eb';
          avatarState.legs_pattern = data.legs_pattern || '';
          avatarState.feet_color = data.feet_color || '#e5e7eb';
          
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
    let lastFocusedInput = null;

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

    document.querySelectorAll('input[type="text"], input[type="password"], textarea').forEach(el => {
      el.addEventListener('focus', () => { lastFocusedInput = el; });
    });

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

    const selectOperatingMode = (selectedMode) => {
      if (selectedMode === espMode) {
        if (selectedMode === 1) {
          let username = document.getElementById('salon-username-input').value.trim();
          if (username === "") {
            alert("Veuillez saisir un pseudonyme pour entrer dans le salon de discussion.");
            return;
          }
          localStorage.setItem('civvi_salon_username', username);
        }
        sessionStorage.setItem('civvi_mode_confirmed', 'true');
        document.getElementById('mode-selection-screen').classList.add('hidden');
      } else {
        let modeName = selectedMode === 1 ? "Salon de Discussion" : "Mode Relais (P2P)";
        let configSsid = document.getElementById('config-ssid').value || "civvi-";
        let newSSID = selectedMode === 1 ? configSsid + "-salon" : configSsid;
        
        let confirmMsg = `L'ESP va redémarrer pour basculer en ${modeName}.\n\nVous devrez vous connecter au réseau Wi-Fi : "${newSSID}" après le redémarrage.\n\nSouhaitez-vous continuer ?`;
        
        if (confirm(confirmMsg)) {
          let statusText = document.getElementById('mode-selection-status');
          if (statusText) statusText.innerText = "Envoi de la commande de changement de mode...";
          
          let fd = new URLSearchParams();
          fd.append('mode', selectedMode);
          
          fetch('/api/set_mode', { method: 'POST', body: fd })
            .then(r => {
              if (r.ok) {
                if (statusText) statusText.innerHTML = `Redémarrage en cours...<br><br>Veuillez vous connecter au réseau Wi-Fi : <strong style="color:var(--primary); font-size:16px;">${newSSID}</strong>`;
                document.getElementById('mode-card-relais').style.pointerEvents = 'none';
                document.getElementById('mode-card-salon').style.pointerEvents = 'none';
              } else {
                alert("Erreur lors du changement de mode.");
                if (statusText) statusText.innerText = "";
              }
            })
            .catch(err => {
              if (statusText) statusText.innerHTML = `Redémarrage en cours...<br><br>Veuillez vous connecter au réseau Wi-Fi : <strong style="color:var(--primary); font-size:16px;">${newSSID}</strong>`;
            });
        }
      }
    };

    window.onload = () => {
      setTimeout(() => {
        let splash = document.getElementById('splash-screen');
        if (splash && !splash.classList.contains('hidden')) {
          splash.classList.add('hidden');
        }
      }, 4000);

      const maintenant = new Date();
      const epochSecondes = Math.floor(maintenant.getTime() / 1000);
      const timezoneOffset = maintenant.getTimezoneOffset() * -60;
      fetch('/api/sync-time', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'time=' + epochSecondes + '&offset=' + timezoneOffset
      });

      fetch('/api/config')
        .then(r => r.json())
        .then(data => {
          document.getElementById('config-ssid').value = data.ssid || "";
          document.getElementById('config-pwd').value = data.pwd || "";
          document.getElementById('config-username').value = data.username || "";
          document.getElementById('config-reboot-msg').value = data.rebootMsg || "";
          if (document.getElementById('config-mode')) document.getElementById('config-mode').value = data.mode !== undefined ? data.mode : "0";

          espMode = data.mode !== undefined ? parseInt(data.mode) : 0;
          if (espMode === 1) {
            document.body.classList.add('salon-mode');
            document.getElementById('header-user').innerText = localStorage.getItem('civvi_salon_username') || "Visiteur";
            document.getElementById('header-ssid').innerText = `Salon : ${data.ssid || "Inconnu"}-salon`;
          } else {
            document.getElementById('header-user').innerText = data.username || "Civvi";
            document.getElementById('header-ssid').innerText = `Réseau : ${data.ssid || "Inconnu"}`;
          }

          document.getElementById('mode-card-relais').classList.remove('current');
          document.getElementById('mode-card-salon').classList.remove('current');
          if (espMode === 1) {
            document.getElementById('mode-card-salon').classList.add('current');
            document.getElementById('salon-username-input').value = localStorage.getItem('civvi_salon_username') || "";
          } else {
            document.getElementById('mode-card-relais').classList.add('current');
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
      setInterval(update, 25000);



      // Initialize badge polling
      setInterval(checkNewFeuilleMessages, 5000);
      checkNewFeuilleMessages();

      // Initialize Avatar and Profile loader
      applyAvatarState();
      loadStructuredProfile();
    };

    const ouvrirOverlay = (idOverlay) => {
      fermerOverlays();
      document.getElementById(idOverlay).classList.add('actif');
      document.getElementById('btn-configuration').style.opacity = '0';
      document.getElementById('btn-configuration').style.pointerEvents = 'none';
      document.getElementById('btn-profil').style.opacity = '0';
      document.getElementById('btn-profil').style.pointerEvents = 'none';
      document.getElementById('btn-messagerie').style.opacity = '0';
      document.getElementById('btn-messagerie').style.pointerEvents = 'none';
      activeTab = idOverlay;
      if (idOverlay === 'ov-messagerie') {
        loadReceivedGraffitis();
      } else if (idOverlay === 'ov-configuration') {
        loadBroadcastHistory();
      }
    };

    const fermerOverlays = (userClicked = false) => {
      let anyActive = false;
      document.querySelectorAll('.overlay').forEach(ov => {
        if (ov.classList.contains('actif')) {
          anyActive = true;
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

      if (userClicked && !anyActive) {
        ouvrirOverlay('ov-lorem');
      }
    };

    const toggleVirtualKeyboard = (e) => {
      if(e) e.stopPropagation();
      let kb = document.getElementById('keyboard-overlay');
      kb.classList.toggle('actif');
      if (kb.classList.contains('actif')) {
        document.querySelectorAll('input[type="text"], input[type="password"], textarea').forEach(el => {
          el.setAttribute('inputmode', 'none');
        });
      } else {
        document.querySelectorAll('input[type="text"], input[type="password"], textarea').forEach(el => {
          el.removeAttribute('inputmode');
        });
      }
    };
    const fermerClavier = (e) => {
      if(e) e.stopPropagation();
      document.getElementById('keyboard-overlay').classList.remove('actif');
      document.querySelectorAll('input[type="text"], input[type="password"], textarea').forEach(el => {
        el.removeAttribute('inputmode');
      });
    };
    const taperLettre = (char) => {
      let input = lastFocusedInput || document.getElementById('msg');
      if (input) {
        input.value += char;
        input.dispatchEvent(new Event('input'));
        input.focus();
      }
    };
    const effacerDernierLettre = () => {
      let input = lastFocusedInput || document.getElementById('msg');
      if (input && input.value.length > 0) {
        input.value = input.value.substring(0, input.value.length - 1);
        input.dispatchEvent(new Event('input'));
        input.focus();
      }
    };
    const simulerFleche = (dir) => {
      let input = lastFocusedInput || document.getElementById('msg');
      if (input) {
        let pos = input.selectionStart;
        if (dir === 'Left' && pos > 0) input.setSelectionRange(pos - 1, pos - 1);
        else if (dir === 'Right' && pos < input.value.length) input.setSelectionRange(pos + 1, pos + 1);
        input.focus();
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
        let username = localStorage.getItem('civvi_salon_username') || "Visiteur";
        url += '?user=' + encodeURIComponent(username);
      }
      
      fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'text/plain; charset=utf-8' },
        body: text
      }).then(() => {
        document.getElementById('msg').value = "";
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

    const startMarquee = () => {
      if (marqueeActive) return;
      
      let actuMarquee = document.getElementById('marquee-actu-text');
      let actuZoom = document.getElementById('ov-actu-marquee');
      
      if (marqueeQueue.length > 0) {
        marqueeActive = true;
        let currentItem = marqueeQueue.shift();
        
        let datePart = "";
        let messagePart = currentItem.texte;
        let match = currentItem.texte.match(/^(\[\d{2}\/\d{2}\s\d{2}:\d{2}\]\s*)(.*)$/);
        if (match) {
          datePart = match[1];
          messagePart = match[2];
        }
        
        let username = currentItem.username || currentItem.nomReseau;
        let normalHtml = `<strong>${username}</strong> : ${currentItem.texte} &nbsp;&nbsp;&nbsp; 🌿`;
        let giantHtml = `
          <span style="font-size: 5vh; vertical-align: middle; color: var(--text-muted); font-family: monospace; margin-right: 15px;">${datePart}</span>
          <span style="font-size: 7vh; vertical-align: middle; color: var(--secondary); font-family:-apple-system, BlinkMacSystemFont, sans-serif; font-weight: bold; margin-right: 30px;">${username} :</span>
          <span style="font-size: 82vh; vertical-align: middle; line-height: 1.1; color: var(--primary); font-family: Georgia, serif; font-style: italic; text-shadow: 0 0 35px rgba(217, 149, 43, 0.6);">${messagePart}</span>
          <span style="font-size: 15vh; vertical-align: middle; color: var(--secondary); margin-left: 20px;">🌿</span>
        `;
        
        actuMarquee.innerHTML = normalHtml;
        actuZoom.innerHTML = giantHtml;
        
        shownMessages.add(currentItem.key);
        
        let textLen = actuMarquee.innerText.length;
        let duration = Math.max(8, textLen * 0.15);
        let zoomDuration = duration * 4;
        
        actuMarquee.style.animation = "none";
        actuZoom.style.animation = "none";
        actuZoom.style.textAlign = "left";
        
        // Trigger reflow
        actuMarquee.offsetHeight;
        actuZoom.offsetHeight;
        
        actuMarquee.style.animation = `defilementD-G ${duration}s linear forwards`;
        actuZoom.style.animation = `defilementD-G ${zoomDuration}s linear forwards`;
        
        actuMarquee.onanimationend = () => {
          marqueeActive = false;
          startMarquee();
        };
      } else {
        actuMarquee.innerHTML = "En attente d'actualité...";
        actuZoom.innerHTML = `<span style="font-size: 10vh; color: var(--primary); font-family: Georgia, serif; font-style: italic; text-shadow: 0 0 25px rgba(217, 149, 43, 0.5);">En attente d'actualité...</span>`;
        actuMarquee.style.animation = "none";
        actuZoom.style.animation = "none";
        actuZoom.style.textAlign = "center";
        actuMarquee.onanimationend = null;
      }
    };

    const update = () => {
      fetch('/messages')
        .then(r => r.json())
        .then(data => {
          currentData = data;
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
            if (cleanMyThought.trim() === "") {
              document.getElementById('my-thought-marquee').innerHTML = `Ma pensée : <em>(aucune)</em>`;
            } else {
              document.getElementById('my-thought-marquee').innerHTML = `Ma pensée : <strong>${myThoughtText}</strong>`;
            }
          }

          if (!firstUpdateDone) {
            firstUpdateDone = true;
            let count = data.length - 1;
            let counterText = document.getElementById('splash-counter-text');
            let statusText = document.getElementById('splash-status-text');
            if (counterText) {
              counterText.innerText = espMode === 1 
                ? "Connexion au salon de discussion..." 
                : `${count} module(s) actif(s) détecté(s) dans les environs`;
            }
            if (statusText) {
              statusText.innerText = espMode === 1 
                ? "Salon de discussion connecté" 
                : "Scan réseau terminé";
            }
            setTimeout(() => {
              let splash = document.getElementById('splash-screen');
              if (splash) splash.classList.add('hidden');
            }, 1500);
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
          let activeOthers = othersData.filter(m => !expiredMacs.has(m.auteur));

          // Sort activeOthers based on sortMode
          if (sortMode === 'fav_first') {
            activeOthers.sort((a, b) => {
              let favA = myFavorites.includes(a.auteur.toLowerCase()) ? 1 : 0;
              let favB = myFavorites.includes(b.auteur.toLowerCase()) ? 1 : 0;
              if (favA !== favB) return favB - favA;
              return b.rssi - a.rssi;
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

            // Determine if signal has disappeared (age > 30s)
            let isExpired = m.age > 30000;
            
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
              if (isExpired) line.className = "esp-line expired" + neighborContrastClass;
              line.id = id;
              line.style.borderColor = color;
              
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

                if (line.classList.contains('expired')) {
                  let count = parseInt(line.dataset.expiredIterations || 0) + 1;
                  line.dataset.expiredIterations = count;
                  if (count >= 2) {
                    expiredMacs.add(m.auteur);
                    line.remove();
                  }
                }
              });

              riviere.appendChild(anim);

              let cvBtn = document.createElement('button');
              cvBtn.className = "cv-case-btn";
              cvBtn.innerHTML = '<svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor" style="display:inline-block; vertical-align:middle; color:var(--text-muted);"><circle cx="12" cy="5" r="3.5" /><rect x="8" y="10" width="8" height="5.5" rx="1" /><rect x="9.5" y="17" width="2" height="4.5" /><rect x="12.5" y="17" width="2" height="4.5" /><rect x="8.5" y="22.5" width="2" height="1" /><rect x="13.5" y="22.5" width="2" height="1" /></svg>';
              cvBtn.title = "Consulter le profil";
              cvBtn.onclick = () => viewPeerCV(m.auteur, m.username || m.nomReseau);

              let voteBtn = document.createElement('button');
              voteBtn.className = "vote-case-btn";
              voteBtn.innerText = "✔️";
              voteBtn.onclick = (e) => {
                showReactionsPicker(e, m.auteur);
              };

              let votesContainer = document.createElement('div');
              votesContainer.className = "votes-display";
              votesContainer.id = "votes-" + id;
              votesContainer.innerHTML = votesHtml;

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
                    .then(() => alert("Graffiti postal envoyé !"));
                }
              };
              let ghostBtn = document.createElement('div');
              ghostBtn.className = "ghost-btn";
              ghostContainer.appendChild(ghostBtn);

              line.appendChild(cvBtn);
              line.appendChild(cartouche);
              line.appendChild(riviere);
              line.appendChild(voteBtn);
              line.appendChild(votesContainer);
              line.appendChild(ghostContainer);
              
              container.appendChild(line);
            } else {
              // Node exists. Handle live signal loss/return.
              if (isExpired) {
                if (!line.classList.contains('expired')) {
                  line.classList.add('expired');
                  line.dataset.expiredIterations = "0";
                }
              } else {
                if (line.classList.contains('expired')) {
                  line.classList.remove('expired');
                  delete line.dataset.expiredIterations;
                }
              }
              
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
            
            // Visually preserve sorting order using CSS flexbox 'order' property to avoid resetting animations
            line.style.order = index;
          });

          // Handle nodes that were completely removed from dataset by C++
          existingIds.forEach(id => {
            if (!newIds.has(id)) {
              let line = document.getElementById(id);
            let macLower = m.auteur.toLowerCase();
            let isFav = myFavorites.includes(macLower);
            let favStar = isFav ? "🌟 " : "";
            let nameDisplay = isFav ? `🌟 ${m.username || m.nomReseau} 🌟` : (m.username || m.nomReseau);
              if (line && !line.classList.contains('expired')) {
                line.classList.add('expired');
                line.dataset.expiredIterations = "0";
              }
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
      
      let headStyle = headColor === 'transparent' ? 'opacity:0.3;' : `background-color:${headColor};`;
      let torsoStyle = `background-color:${torsoColor};`;
      let legsStyle = `background-color:${legsColor};`;
      let feetStyle = `background-color:${feetColor};`;

      let torsoPatternClass = torsoPattern ? ` ${torsoPattern}` : '';
      let legsPatternClass = legsPattern ? ` ${legsPattern}` : '';

      let favText = myFavorites.includes(mac.toLowerCase()) ? "Retirer des favoris ⭐️" : "Ajouter aux favoris 🌟";

      let html = `
        <div style="text-align:center;">
          <h1 style="font-family:Georgia, serif; font-size:22px; color:var(--primary); margin-bottom:4px;">${username}</h1>
          <div style="font-size:11px; color:var(--text-muted); font-family:monospace; margin-bottom:15px;">MAC : ${mac}</div>
          
          <div style="display:flex; justify-content:center; gap:8px; margin-bottom:20px;">
            <button class="btn" style="padding:4px 10px; font-size:11px;" onclick="toggleFavoritePeer('${mac}', '${username}')">${favText}</button>
          </div>

          <div style="display:flex; align-items:center; justify-content:center; background:var(--surface-low); border:1px solid var(--border); padding:20px; border-radius:8px;">
            <!-- Avatar Display -->
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
      let mode = document.getElementById('config-mode') ? document.getElementById('config-mode').value : espMode;

      if (ssid === "") return alert("Le SSID ne peut pas être vide !");
      let fd = new URLSearchParams();
      fd.append('ssid', ssid);
      fd.append('pwd', pwd);
      fd.append('username', username);
      fd.append('rebootMsg', rebootMsg);
      fd.append('mode', mode);

      fetch('/api/config', { method: 'POST', body: fd })
        .then(() => {
          let newSSID = mode === "1" ? ssid + "-salon" : ssid;
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
</script>
</body>
</html>
)rawliteral"; // Fin de la chaine brute contenant le code HTML/CSS/JS de la page web.

// ===== PORTAIL CAPTIF (Méthode Agressive) ===== // Affectation de valeur a une variable ou modification d'etat.
void sendNoCacheHeaders() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
  server.sendHeader("Pragma", "no-cache"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
  server.sendHeader("Expires", "-1"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
}

// ===== ROUTAGE DU TELEPROMPTEUR ===== // Affectation de valeur a une variable ou modification d'etat.
void handleRoot() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  sendNoCacheHeaders(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  server.send_P(200, "text/html", htmlPage); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
}

String obtenirTempsFormate() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  time_t now = time(nullptr); // Affectation de valeur a une variable ou modification d'etat.
  if (now < 1577836800) { // Non synchronisé (avant 2020) // Verification d'une condition logique.
    return ""; // Instruction d'execution.
  }
  struct tm timeinfo; // Instruction d'execution.
  localtime_r(&now, &timeinfo); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  char buf[30]; // Instruction d'execution.
  strftime(buf, sizeof(buf), "[%d/%m %H:%M]", &timeinfo); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  return String(buf); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
}

// Ajoute un message diffusé par notre module dans l'historique local // Instruction d'execution.
// (/history.txt) Gère une rotation FIFO à 50 entrées max pour éviter la // Appel d'une fonction interne ou d'une memoire de bibliotheque.
// saturation de la mémoire flash // Instruction d'execution.
void ajouterAHistory(String texte) { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  // Récupération de l'heure actuelle formatée // Instruction d'execution.
  String t = obtenirTempsFormate(); // Affectation de valeur a une variable ou modification d'etat.
  if (t.length() > 0) { // Verification d'une condition logique.
    t = t.substring(1, t.length() - 1); // Enlever les crochets pour l'affichage // Affectation de valeur a une variable ou modification d'etat.
  } else { // Instruction d'execution.
    t = "--/-- --:--"; // Affectation de valeur a une variable ou modification d'etat.
  }

  std::vector<String> entries; // Tableau dynamique C++ stockant les donnees du programme.
  // Lire l'historique existant stocké sur LittleFS // Instruction d'execution.
  if (LittleFS.exists("/history.txt")) { // Verification d'une condition logique.
    File f = LittleFS.open("/history.txt", "r"); // Gestion des fichiers internes de stockage LittleFS.
    if (f) { // Verification d'une condition logique.
      while (f.available()) { // Execution d'une boucle iterative.
        String line = f.readStringUntil('\n'); // Affectation de valeur a une variable ou modification d'etat.
        line.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        if (line.length() > 0) { // Verification d'une condition logique.
          entries.push_back(line); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        }
      }
      f.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    }
  }

  // Ajouter la nouvelle entrée formatée : message|||heure // Instruction d'execution.
  String newLine = texte + "|||" + t; // Affectation de valeur a une variable ou modification d'etat.
  entries.push_back(newLine); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

  // Rotation FIFO : conserver au maximum 50 messages // Instruction d'execution.
  size_t maxEntries = 50; // Affectation de valeur a une variable ou modification d'etat.
  if (entries.size() > maxEntries) { // Verification d'une condition logique.
    entries.erase(entries.begin(), // Appel d'une fonction interne ou d'une memoire de bibliotheque.
                  entries.begin() + (entries.size() - maxEntries)); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  }

  // Enregistrer le nouvel historique mis à jour sur LittleFS // Instruction d'execution.
  File f = LittleFS.open("/history.txt", "w"); // Gestion des fichiers internes de stockage LittleFS.
  if (f) { // Verification d'une condition logique.
    for (auto &entry : entries) { // Execution d'une boucle iterative.
      f.println(entry); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    }
    f.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  }
}

void ajouterAuJournal(String auteur, String nomReseau, String texte) { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  String t = obtenirTempsFormate(); // Affectation de valeur a une variable ou modification d'etat.
  if (t.length() > 0) { // Verification d'une condition logique.
    t = t.substring(1, t.length() - 1); // enlever les crochets // Affectation de valeur a une variable ou modification d'etat.
  } else { // Instruction d'execution.
    t = "--/-- --:--"; // Affectation de valeur a une variable ou modification d'etat.
  }

  std::vector<String> entries; // Tableau dynamique C++ stockant les donnees du programme.
  if (LittleFS.exists("/journal.txt")) { // Verification d'une condition logique.
    File f = LittleFS.open("/journal.txt", "r"); // Gestion des fichiers internes de stockage LittleFS.
    if (f) { // Verification d'une condition logique.
      while (f.available()) { // Execution d'une boucle iterative.
        String line = f.readStringUntil('\n'); // Affectation de valeur a une variable ou modification d'etat.
        line.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        if (line.length() > 0) { // Verification d'une condition logique.
          entries.push_back(line); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        }
      }
      f.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    }
  }

  bool isDuplicate = false; // Affectation de valeur a une variable ou modification d'etat.
  for (const auto &entry : entries) { // Execution d'une boucle iterative.
    int s1 = entry.indexOf("|||"); // Affectation de valeur a une variable ou modification d'etat.
    if (s1 > 0) { // Verification d'une condition logique.
      String entryAuteur = entry.substring(0, s1); // Affectation de valeur a une variable ou modification d'etat.
      if (entryAuteur == auteur) { // Verification d'une condition logique.
        int s2 = entry.indexOf("|||", s1 + 3); // Affectation de valeur a une variable ou modification d'etat.
        if (s2 > 0) { // Verification d'une condition logique.
          int s3 = entry.indexOf("|||", s2 + 3); // Affectation de valeur a une variable ou modification d'etat.
          String entryTexte = // Affectation de valeur a une variable ou modification d'etat.
              (s3 > 0) ? entry.substring(s2 + 3, s3) : entry.substring(s2 + 3); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
          if (entryTexte == texte) { // Verification d'une condition logique.
            isDuplicate = true; // Affectation de valeur a une variable ou modification d'etat.
            break; // Instruction d'execution.
          }
        }
      }
    }
  }

  if (isDuplicate) { // Verification d'une condition logique.
    return; // Instruction d'execution.
  }

  String newLine = auteur + "|||" + nomReseau + "|||" + texte + "|||" + t; // Affectation de valeur a une variable ou modification d'etat.
  entries.push_back(newLine); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

  size_t totalSpace = LittleFS.totalBytes(); // Affectation de valeur a une variable ou modification d'etat.
  size_t usedSpace = LittleFS.usedBytes(); // Affectation de valeur a une variable ou modification d'etat.
  size_t freeSpace = (totalSpace > usedSpace) ? (totalSpace - usedSpace) : 0; // Affectation de valeur a une variable ou modification d'etat.
  size_t maxEntries = 100; // Affectation de valeur a une variable ou modification d'etat.
  if (freeSpace < 32768) { // Verification d'une condition logique.
    maxEntries = 10; // Affectation de valeur a une variable ou modification d'etat.
  } else if (freeSpace < 65536) { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    maxEntries = 30; // Affectation de valeur a une variable ou modification d'etat.
  } else if (freeSpace < 131072) { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    maxEntries = 60; // Affectation de valeur a une variable ou modification d'etat.
  }

  if (entries.size() > maxEntries) { // Verification d'une condition logique.
    entries.erase(entries.begin(), // Appel d'une fonction interne ou d'une memoire de bibliotheque.
                  entries.begin() + (entries.size() - maxEntries)); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  }

  File f = LittleFS.open("/journal.txt", "w"); // Gestion des fichiers internes de stockage LittleFS.
  if (f) { // Verification d'une condition logique.
    for (auto &entry : entries) { // Execution d'une boucle iterative.
      f.println(entry); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    }
    f.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  }
}

void handleTimeSync() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (server.hasArg("time")) { // Verification d'une condition logique.
    long t = server.arg("time").substring(0, 10).toInt(); // Affectation de valeur a une variable ou modification d'etat.
    long offset = server.arg("offset").toInt(); // Affectation de valeur a une variable ou modification d'etat.

    struct timeval tv; // Instruction d'execution.
    tv.tv_sec = t + offset; // Affectation de valeur a une variable ou modification d'etat.
    tv.tv_usec = 0; // Affectation de valeur a une variable ou modification d'etat.
    settimeofday(&tv, NULL); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

    server.send(200, "text/plain", "OK"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    Serial.print("Horloge ESP32 mise à jour : "); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    time_t now = time(nullptr); // Affectation de valeur a une variable ou modification d'etat.
    Serial.println(ctime(&now)); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  } else { // Instruction d'execution.
    server.send(400, "text/plain", "Requête incorrecte"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
  }
}

void handlePostVote() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (server.hasArg("cible") && server.hasArg("emoticon")) { // Verification d'une condition logique.
    String cible = server.arg("cible"); // Affectation de valeur a une variable ou modification d'etat.
    String emoticon = server.arg("emoticon"); // Affectation de valeur a une variable ou modification d'etat.

    String paquet = "V|" + cible + "|" + monID + "|" + emoticon; // Affectation de valeur a une variable ou modification d'etat.
    esp_now_send(broadcastAddress, (uint8_t *)paquet.c_str(), paquet.length()); // Transmission sans fil d'un paquet de donnees via le protocole ESP-NOW.

    votesMap[cible][emoticon]++; // Instruction d'execution.

    server.send(200, "text/plain", "OK"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
  } else { // Instruction d'execution.
    server.send(400, "text/plain", "Paramètres manquants"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
  }
}

void handleGetNotifications() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  String json = "["; // Affectation de valeur a une variable ou modification d'etat.
  for (int i = 0; i < voteNotifications.size(); i++) { // Execution d'une boucle iterative.
    if (i > 0) // Verification d'une condition logique.
      json += ","; // Affectation de valeur a une variable ou modification d'etat.
    json += // Affectation de valeur a une variable ou modification d'etat.
        "{\"voteur\":\"" + voteNotifications[i].voteur + "\",\"emoticon\":\"" + // Instruction d'execution.
        voteNotifications[i].emoticon + // Instruction d'execution.
        "\",\"age\":" + String(millis() - voteNotifications[i].timeReceived) + // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        "}"; // Instruction d'execution.
  }
  json += "]"; // Affectation de valeur a une variable ou modification d'etat.
  server.send(200, "application/json", json); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
}

void handleGetJournal() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (!LittleFS.exists("/journal.txt")) { // Verification d'une condition logique.
    server.send(200, "application/json", "[]"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    return; // Instruction d'execution.
  }
  File f = LittleFS.open("/journal.txt", "r"); // Gestion des fichiers internes de stockage LittleFS.
  if (!f) { // Verification d'une condition logique.
    server.send(500, "text/plain", "Erreur lecture journal"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    return; // Instruction d'execution.
  }

  String json = "["; // Affectation de valeur a une variable ou modification d'etat.
  bool first = true; // Affectation de valeur a une variable ou modification d'etat.
  while (f.available()) { // Execution d'une boucle iterative.
    String line = f.readStringUntil('\n'); // Affectation de valeur a une variable ou modification d'etat.
    line.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    if (line.length() > 0) { // Verification d'une condition logique.
      String pieces[4]; // Instruction d'execution.
      int count = 0; // Affectation de valeur a une variable ou modification d'etat.
      int startIdx = 0; // Affectation de valeur a une variable ou modification d'etat.
      while (startIdx < line.length() && count < 4) { // Execution d'une boucle iterative.
        int nextIdx = line.indexOf("|||", startIdx); // Affectation de valeur a une variable ou modification d'etat.
        if (nextIdx == -1) { // Verification d'une condition logique.
          pieces[count++] = line.substring(startIdx); // Affectation de valeur a une variable ou modification d'etat.
          break; // Instruction d'execution.
        }
        pieces[count++] = line.substring(startIdx, nextIdx); // Affectation de valeur a une variable ou modification d'etat.
        startIdx = nextIdx + 3; // Affectation de valeur a une variable ou modification d'etat.
      }
      if (count >= 3) { // Verification d'une condition logique.
        if (!first) // Verification d'une condition logique.
          json += ","; // Affectation de valeur a une variable ou modification d'etat.
        String auteur = pieces[0]; // Affectation de valeur a une variable ou modification d'etat.
        String nom = pieces[1]; // Affectation de valeur a une variable ou modification d'etat.
        String texte = pieces[2]; // Affectation de valeur a une variable ou modification d'etat.
        String timestamp = (count >= 4) ? pieces[3] : ""; // Affectation de valeur a une variable ou modification d'etat.

        auteur.replace("\"", "\\\""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        nom.replace("\"", "\\\""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        texte.replace("\"", "\\\""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

        json += "{\"author\":\"" + auteur + "\",\"ssid\":\"" + nom + // Affectation de valeur a une variable ou modification d'etat.
                "\",\"text\":\"" + texte + "\",\"time\":\"" + timestamp + "\"}"; // Instruction d'execution.
        first = false; // Affectation de valeur a une variable ou modification d'etat.
      }
    }
  }
  f.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  json += "]"; // Affectation de valeur a une variable ou modification d'etat.
  server.send(200, "application/json", json); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
}

void handleClearJournal() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (LittleFS.exists("/journal.txt")) { // Verification d'une condition logique.
    LittleFS.remove("/journal.txt"); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  }
  server.send(200, "text/plain", "OK"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
}

// Lit le fichier /history.txt et le retourne sous forme de tableau JSON // Instruction d'execution.
// Format JSON renvoyé : [{"text": "Message", "time": "12/06 15:30"}, ...] // Instruction d'execution.
void handleGetHistory() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (!LittleFS.exists("/history.txt")) { // Verification d'une condition logique.
    server.send(200, "application/json", "[]"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    return; // Instruction d'execution.
  }
  File f = LittleFS.open("/history.txt", "r"); // Gestion des fichiers internes de stockage LittleFS.
  if (!f) { // Verification d'une condition logique.
    server.send(500, "text/plain", "Erreur lecture historique"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    return; // Instruction d'execution.
  }

  String json = "["; // Affectation de valeur a une variable ou modification d'etat.
  bool first = true; // Affectation de valeur a une variable ou modification d'etat.
  while (f.available()) { // Execution d'une boucle iterative.
    String line = f.readStringUntil('\n'); // Affectation de valeur a une variable ou modification d'etat.
    line.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    if (line.length() > 0) { // Verification d'une condition logique.
      int sep = line.indexOf("|||"); // Affectation de valeur a une variable ou modification d'etat.
      if (sep > 0) { // Verification d'une condition logique.
        if (!first) // Verification d'une condition logique.
          json += ","; // Affectation de valeur a une variable ou modification d'etat.
        String txt = line.substring(0, sep); // Affectation de valeur a une variable ou modification d'etat.
        String timestamp = line.substring(sep + 3); // Affectation de valeur a une variable ou modification d'etat.
        // Échappement des guillemets pour éviter de casser le JSON // Instruction d'execution.
        txt.replace("\"", "\\\""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        json += "{\"text\":\"" + txt + "\",\"time\":\"" + timestamp + "\"}"; // Affectation de valeur a une variable ou modification d'etat.
        first = false; // Affectation de valeur a une variable ou modification d'etat.
      }
    }
  }
  f.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  json += "]"; // Affectation de valeur a une variable ou modification d'etat.
  server.send(200, "application/json", json); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
}

// Supprime définitivement le fichier d'historique local /history.txt // Instruction d'execution.
void handleClearHistory() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (LittleFS.exists("/history.txt")) { // Verification d'une condition logique.
    LittleFS.remove("/history.txt"); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  }
  server.send(200, "text/plain", "OK"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
}

void handleSend() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (server.hasArg("plain")) { // Verification d'une condition logique.
    String messageText = server.arg("plain"); // Affectation de valeur a une variable ou modification d'etat.
    if (fonctionnementMode == 1) { // Salon Mode // Verification d'une condition logique.
      String username = server.hasArg("user") ? server.arg("user") : "Anonyme"; // Affectation de valeur a une variable ou modification d'etat.
      username.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      if (username.length() == 0) username = "Anonyme"; // Verification d'une condition logique.
      
      String timeStr = obtenirTempsFormate(); // Affectation de valeur a une variable ou modification d'etat.
      if (timeStr.length() > 0) { // Verification d'une condition logique.
        if (!messageText.startsWith("[")) { // Verification d'une condition logique.
          messageText = timeStr + " " + messageText; // Affectation de valeur a une variable ou modification d'etat.
        }
      }

      SalonMessage sm; // Instruction d'execution.
      sm.auteur = username; // Affectation de valeur a une variable ou modification d'etat.
      sm.texte = messageText; // Affectation de valeur a une variable ou modification d'etat.
      sm.timestamp = millis(); // Affectation de valeur a une variable ou modification d'etat.

      salonMessages.push_back(sm); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      if (salonMessages.size() > 40) { // Verification d'une condition logique.
        salonMessages.erase(salonMessages.begin()); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      }
    } else { // Relais Mode (P2P) // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      String timeStr = obtenirTempsFormate(); // Affectation de valeur a une variable ou modification d'etat.
      if (timeStr.length() > 0) { // Verification d'une condition logique.
        if (!messageText.startsWith("[")) { // Verification d'une condition logique.
          messageText = timeStr + " " + messageText; // Affectation de valeur a une variable ou modification d'etat.
        }
      }
      maPensee = messageText; // Affectation de valeur a une variable ou modification d'etat.
      sauvegarderPensees(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      ajouterAuJournal(monNomUtilisateur, monNomReseau, maPensee); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      ajouterAHistory(maPensee); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      envoyerTexteLong(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    }
  }
  server.send(200, "text/plain", "OK"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
}

void handleMessages() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (fonctionnementMode == 1) { // Salon Mode // Verification d'une condition logique.
    String json = "["; // Affectation de valeur a une variable ou modification d'etat.
    String hostSSID = monNomReseau + "-salon"; // Affectation de valeur a une variable ou modification d'etat.
    json += "{\"auteur\":\"host\",\"nomReseau\":\"" + hostSSID + "\",\"username\":\"Salon\",\"texte\":\"\",\"rssi\":0,\"age\":0,\"contrast\":0}"; // Affectation de valeur a une variable ou modification d'etat.
    
    for (int i = (int)salonMessages.size() - 1; i >= 0; i--) { // Execution d'une boucle iterative.
      json += ","; // Affectation de valeur a une variable ou modification d'etat.
      String escapedText = salonMessages[i].texte; // Affectation de valeur a une variable ou modification d'etat.
      escapedText.replace("\"", "\\\""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      escapedText.replace("\n", "\\n"); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      escapedText.replace("\r", ""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      String escapedAuteur = salonMessages[i].auteur; // Affectation de valeur a une variable ou modification d'etat.
      escapedAuteur.replace("\"", "\\\""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      
      String uniqueAuteurId = escapedAuteur + "_" + String(salonMessages[i].timestamp); // Affectation de valeur a une variable ou modification d'etat.
      
      json += "{\"auteur\":\"" + uniqueAuteurId + "\",\"nomReseau\":\"Salon\",\"username\":\"" + escapedAuteur + "\",\"texte\":\"" + escapedText + "\",\"rssi\":0,\"age\":0,\"contrast\":0}"; // Affectation de valeur a une variable ou modification d'etat.
    }
    json += "]"; // Affectation de valeur a une variable ou modification d'etat.
    server.send(200, "application/json", json); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    return; // Instruction d'execution.
  }

  String myVotesJson = "{"; // Affectation de valeur a une variable ou modification d'etat.
  if (votesMap.find(monID) != votesMap.end()) { // Verification d'une condition logique.
    bool firstVote = true; // Affectation de valeur a une variable ou modification d'etat.
    for (auto const &[emo, val] : votesMap[monID]) { // Execution d'une boucle iterative.
      if (!firstVote) // Verification d'une condition logique.
        myVotesJson += ","; // Affectation de valeur a une variable ou modification d'etat.
      myVotesJson += "\"" + emo + "\":" + String(val); // Affectation de valeur a une variable ou modification d'etat.
      firstVote = false; // Affectation de valeur a une variable ou modification d'etat.
    }
  }
  myVotesJson += "}"; // Affectation de valeur a une variable ou modification d'etat.

  String escapedMyPensee = maPensee; // Affectation de valeur a une variable ou modification d'etat.
  escapedMyPensee.replace("\"", "\\\""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  escapedMyPensee.replace("\n", "\\n"); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  escapedMyPensee.replace("\r", ""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

  String json = "["; // Affectation de valeur a une variable ou modification d'etat.
  json += "{\"auteur\":\"" + monID + "\",\"nomReseau\":\"" + monNomReseau + // Affectation de valeur a une variable ou modification d'etat.
          "\",\"username\":\"" + monNomUtilisateur + "\",\"texte\":\"" + // Instruction d'execution.
          escapedMyPensee + "\",\"rssi\":0,\"age\":0,\"votes\":" + myVotesJson +  // Instruction d'execution.
          ",\"contrast\":" + String(monContraste) + "}"; // Appel d'une fonction interne ou d'une memoire de bibliotheque.

  for (int i = 0; i < messages.size(); i++) { // Execution d'une boucle iterative.
    String ssidPart = messages[i].nomReseau; // Affectation de valeur a une variable ou modification d'etat.
    String userPart = messages[i].nomReseau; // Affectation de valeur a une variable ou modification d'etat.
    int sepIdx = messages[i].nomReseau.indexOf("::"); // Affectation de valeur a une variable ou modification d'etat.
    if (sepIdx > 0) { // Verification d'une condition logique.
      ssidPart = messages[i].nomReseau.substring(0, sepIdx); // Affectation de valeur a une variable ou modification d'etat.
      userPart = messages[i].nomReseau.substring(sepIdx + 2); // Affectation de valeur a une variable ou modification d'etat.
    }

    unsigned long age = millis() - messages[i].dernierContact; // Affectation de valeur a une variable ou modification d'etat.

    String votesJson = "{"; // Affectation de valeur a une variable ou modification d'etat.
    if (votesMap.find(messages[i].auteur) != votesMap.end()) { // Verification d'une condition logique.
      bool firstVote = true; // Affectation de valeur a une variable ou modification d'etat.
      for (auto const &[emo, val] : votesMap[messages[i].auteur]) { // Execution d'une boucle iterative.
        if (!firstVote) // Verification d'une condition logique.
          votesJson += ","; // Affectation de valeur a une variable ou modification d'etat.
        votesJson += "\"" + emo + "\":" + String(val); // Affectation de valeur a une variable ou modification d'etat.
        firstVote = false; // Affectation de valeur a une variable ou modification d'etat.
      }
    }
    votesJson += "}"; // Affectation de valeur a une variable ou modification d'etat.

    String escapedPeerText = messages[i].texte; // Affectation de valeur a une variable ou modification d'etat.
    escapedPeerText.replace("\"", "\\\""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    escapedPeerText.replace("\n", "\\n"); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    escapedPeerText.replace("\r", ""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

    json += ",{\"auteur\":\"" + messages[i].auteur + "\",\"nomReseau\":\"" + // Affectation de valeur a une variable ou modification d'etat.
            ssidPart + "\",\"username\":\"" + userPart + "\",\"texte\":\"" + // Instruction d'execution.
            escapedPeerText + "\",\"rssi\":" + String(messages[i].rssi) + // Appel d'une fonction interne ou d'une memoire de bibliotheque.
            ",\"age\":" + String(age) + ",\"votes\":" + votesJson +  // Appel d'une fonction interne ou d'une memoire de bibliotheque.
            ",\"contrast\":" + String(messages[i].contrast) + "}"; // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  }
  json += "]"; // Affectation de valeur a une variable ou modification d'etat.
  server.send(200, "application/json", json); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
}

void handleGetNotes() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (LittleFS.exists("/notes.md")) { // Verification d'une condition logique.
    File file = LittleFS.open("/notes.md", "r"); // Gestion des fichiers internes de stockage LittleFS.
    server.streamFile(file, "text/plain"); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    file.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  } else { // Instruction d'execution.
    server.send(200, "text/plain", ""); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
  }
}

void handlePostNotes() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (server.hasArg("plain")) { // Verification d'une condition logique.
    File file = LittleFS.open("/notes.md", "w"); // Gestion des fichiers internes de stockage LittleFS.
    if (file) { // Verification d'une condition logique.
      file.print(server.arg("plain")); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      file.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      server.send(200, "text/plain", "OK"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    } else { // Instruction d'execution.
      server.send(500, "text/plain", "Erreur fichier"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    }
  } else { // Instruction d'execution.
    server.send(400, "text/plain", "Vide"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
  }
}

void handleNotFound() { // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  String path = server.uri(); // Affectation de valeur a une variable ou modification d'etat.
  if (LittleFS.exists(path)) { // Verification d'une condition logique.
    handleStaticFile(path, false); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    return; // Instruction d'execution.
  }
  server.send(404, "text/plain", "404: Not Found"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
}

// ===== INITIALISATION ===== // Affectation de valeur a une variable ou modification d'etat.
void setup() { // Fonction principale d'initialisation de la carte ESP32 lancee une seule fois.
  Serial.begin(115200); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  delay(500); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  Serial.println("\n--- Initialisation Civvi Hybride v2 ---"); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

  initFS(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  {
    File f = LittleFS.open("/index.html", "w"); // Gestion des fichiers internes de stockage LittleFS.
    if (f) { // Verification d'une condition logique.
      f.print(htmlPage); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      f.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      Serial.println("Fichier /index.html mis a jour en local"); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    }
  }
  chargerConfig(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

  // Mode Access Point ET Station (Nécessaire pour ESP-NOW en broadcast sur // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  // certains coeurs) // Instruction d'execution.
  WiFi.mode(WIFI_AP_STA); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0)); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

  String activeSSID = monNomReseau; // Affectation de valeur a une variable ou modification d'etat.
  if (fonctionnementMode == 1) { // Verification d'une condition logique.
    activeSSID += "-salon"; // Affectation de valeur a une variable ou modification d'etat.
  }

  if (monMotDePasse.length() >= 8) { // Verification d'une condition logique.
    WiFi.softAP(activeSSID.c_str(), monMotDePasse.c_str(), 1); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  } else { // Instruction d'execution.
    WiFi.softAP(activeSSID.c_str(), "", 1); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  }

  Serial.print("Réseau créé : "); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  Serial.println(activeSSID); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

  // Fixer le channel // Instruction d'execution.
  if (esp_wifi_set_promiscuous(true) != ESP_OK) // Verification d'une condition logique.
    Serial.println("Erreur mode promiscuous"); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  if (esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE) != ESP_OK) // Verification d'une condition logique.
    Serial.println("Erreur configuration canal"); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  esp_wifi_set_promiscuous(false); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

  monID = WiFi.macAddress(); // Affectation de valeur a une variable ou modification d'etat.
  monID.replace(":", ""); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  monID.toLowerCase(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  chargerPensees(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

  if (esp_now_init() == ESP_OK) { // Verification d'une condition logique.
    memcpy(peerInfo.peer_addr, broadcastAddress, 6); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    peerInfo.channel = 1; // Affectation de valeur a une variable ou modification d'etat.
    peerInfo.encrypt = false; // Affectation de valeur a une variable ou modification d'etat.
    esp_now_add_peer(&peerInfo); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    esp_now_register_recv_cb(onReceive); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  }

  // Routes Web // Instruction d'execution.
  server.on("/", HTTP_GET, handleRoot); // Enregistrement d'un point d'acces d'API Web (URL de routage).

  // Routes Téléprompteur // Instruction d'execution.
  server.on("/send", HTTP_POST, handleSend); // Enregistrement d'un point d'acces d'API Web (URL de routage).
  server.on("/messages", HTTP_GET, handleMessages); // Enregistrement d'un point d'acces d'API Web (URL de routage).

  // Routes Notes // Instruction d'execution.
  server.on("/notes", HTTP_GET, handleGetNotes); // Enregistrement d'un point d'acces d'API Web (URL de routage).
  server.on("/notes", HTTP_POST, handlePostNotes); // Enregistrement d'un point d'acces d'API Web (URL de routage).

  // Routes Config WiFi // Instruction d'execution.
  server.on("/api/config", HTTP_GET, handleGetConfig); // Enregistrement d'un point d'acces d'API Web (URL de routage).
  server.on("/api/config", HTTP_POST, handlePostConfig); // Enregistrement d'un point d'acces d'API Web (URL de routage).

  server.on("/api/set_mode", HTTP_POST, []() { // Enregistrement d'un point d'acces d'API Web (URL de routage).
    if (server.hasArg("mode")) { // Verification d'une condition logique.
      fonctionnementMode = server.arg("mode").toInt(); // Affectation de valeur a une variable ou modification d'etat.
      File f = LittleFS.open(CONFIG_FILE, "w"); // Gestion des fichiers internes de stockage LittleFS.
      if (f) { // Verification d'une condition logique.
        f.println(monNomReseau); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        f.println(monMotDePasse); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        f.println(monMessageReboot); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        f.println(monNomUtilisateur); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        f.println(String(fonctionnementMode)); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        f.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        server.send(200, "text/plain", "OK"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
        delay(500); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        ESP.restart(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      } else { // Instruction d'execution.
        server.send(500, "text/plain", "Erreur écriture"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
      }
    } else { // Instruction d'execution.
      server.send(400, "text/plain", "Paramètre manquant"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    }
  }); // Instruction d'execution.

  server.on("/api/my_contrast", HTTP_POST, []() { // Enregistrement d'un point d'acces d'API Web (URL de routage).
    if (server.hasArg("contrast")) { // Verification d'une condition logique.
      monContraste = server.arg("contrast").toInt(); // Affectation de valeur a une variable ou modification d'etat.
      File f = LittleFS.open("/contrast.txt", "w"); // Gestion des fichiers internes de stockage LittleFS.
      if (f) { // Verification d'une condition logique.
        f.print(String(monContraste)); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        f.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      }
      server.send(200, "text/plain", "OK"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    } else { // Instruction d'execution.
      server.send(400, "text/plain", "Missing contrast"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    }
  }); // Instruction d'execution.

  server.on("/api/trigger_broadcast", HTTP_POST, []() { // Enregistrement d'un point d'acces d'API Web (URL de routage).
    envoyerTexteLong(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    server.send(200, "text/plain", "OK"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
  }); // Instruction d'execution.

  server.on("/api/favorites", HTTP_GET, []() { // Enregistrement d'un point d'acces d'API Web (URL de routage).
    String json = "["; // Affectation de valeur a une variable ou modification d'etat.
    for (size_t i = 0; i < favorites.size(); i++) { // Execution d'une boucle iterative.
      if (i > 0) json += ","; // Verification d'une condition logique.
      json += "\"" + favorites[i] + "\""; // Affectation de valeur a une variable ou modification d'etat.
    }
    json += "]"; // Affectation de valeur a une variable ou modification d'etat.
    server.send(200, "application/json", json); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
  }); // Instruction d'execution.

  server.on("/api/favorites/toggle", HTTP_POST, []() { // Enregistrement d'un point d'acces d'API Web (URL de routage).
    if (server.hasArg("mac")) { // Verification d'une condition logique.
      String mac = server.arg("mac"); // Affectation de valeur a une variable ou modification d'etat.
      mac.toLowerCase(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      mac.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      bool found = false; // Affectation de valeur a une variable ou modification d'etat.
      for (auto it = favorites.begin(); it != favorites.end(); ++it) { // Execution d'une boucle iterative.
        if (*it == mac) { // Verification d'une condition logique.
          favorites.erase(it); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
          found = true; // Affectation de valeur a une variable ou modification d'etat.
          break; // Instruction d'execution.
        }
      }
      if (!found) favorites.push_back(mac); // Verification d'une condition logique.
      File f = LittleFS.open("/favorites.txt", "w"); // Gestion des fichiers internes de stockage LittleFS.
      if (f) { // Verification d'une condition logique.
        for (String m : favorites) f.println(m); // Execution d'une boucle iterative.
        f.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      }
      server.send(200, "text/plain", "OK"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    } else { // Instruction d'execution.
      server.send(400, "text/plain", "Missing MAC"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    }
  }); // Instruction d'execution.

  server.on("/api/banned", HTTP_GET, []() { // Enregistrement d'un point d'acces d'API Web (URL de routage).
    String json = "["; // Affectation de valeur a une variable ou modification d'etat.
    for (size_t i = 0; i < banned.size(); i++) { // Execution d'une boucle iterative.
      if (i > 0) json += ","; // Verification d'une condition logique.
      json += "\"" + banned[i] + "\""; // Affectation de valeur a une variable ou modification d'etat.
    }
    json += "]"; // Affectation de valeur a une variable ou modification d'etat.
    server.send(200, "application/json", json); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
  }); // Instruction d'execution.

  server.on("/api/banned/toggle", HTTP_POST, []() { // Enregistrement d'un point d'acces d'API Web (URL de routage).
    if (server.hasArg("mac")) { // Verification d'une condition logique.
      String mac = server.arg("mac"); // Affectation de valeur a une variable ou modification d'etat.
      mac.toLowerCase(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      mac.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      bool found = false; // Affectation de valeur a une variable ou modification d'etat.
      for (auto it = banned.begin(); it != banned.end(); ++it) { // Execution d'une boucle iterative.
        if (*it == mac) { // Verification d'une condition logique.
          banned.erase(it); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
          found = true; // Affectation de valeur a une variable ou modification d'etat.
          break; // Instruction d'execution.
        }
      }
      if (!found) banned.push_back(mac); // Verification d'une condition logique.
      File f = LittleFS.open("/banned.txt", "w"); // Gestion des fichiers internes de stockage LittleFS.
      if (f) { // Verification d'une condition logique.
        for (String m : banned) f.println(m); // Execution d'une boucle iterative.
        f.close(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      }
      server.send(200, "text/plain", "OK"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    } else { // Instruction d'execution.
      server.send(400, "text/plain", "Missing MAC"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    }
  }); // Instruction d'execution.

  server.on("/api/profile", HTTP_GET, handleGetProfile); // Enregistrement d'un point d'acces d'API Web (URL de routage).
  server.on("/api/profile", HTTP_POST, handlePostProfile); // Enregistrement d'un point d'acces d'API Web (URL de routage).

  server.on("/api/get_cv", HTTP_GET, []() { // Enregistrement d'un point d'acces d'API Web (URL de routage).
    if (server.hasArg("mac")) { // Verification d'une condition logique.
      String targetMac = server.arg("mac"); // Affectation de valeur a une variable ou modification d'etat.
      targetMac.toLowerCase(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      targetMac.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      if (cvsStockes.find(targetMac) != cvsStockes.end()) { // Verification d'une condition logique.
        server.send(200, "text/plain", cvsStockes[targetMac]); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
      } else { // Instruction d'execution.
        server.send(202, "text/plain", "Pending"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
      }
    } else { // Instruction d'execution.
      server.send(400, "text/plain", "Missing MAC"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    }
  }); // Instruction d'execution.

  server.on("/api/request_cv", HTTP_POST, []() { // Enregistrement d'un point d'acces d'API Web (URL de routage).
    if (server.hasArg("mac")) { // Verification d'une condition logique.
      String targetMac = server.arg("mac"); // Affectation de valeur a une variable ou modification d'etat.
      targetMac.toLowerCase(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      targetMac.trim(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      String paquet = "R|" + targetMac + "|" + monID; // Affectation de valeur a une variable ou modification d'etat.
      esp_now_send(broadcastAddress, (uint8_t *)paquet.c_str(), paquet.length()); // Transmission sans fil d'un paquet de donnees via le protocole ESP-NOW.
      server.send(200, "text/plain", "Requested"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    } else { // Instruction d'execution.
      server.send(400, "text/plain", "Missing MAC"); // Envoi de la reponse HTTP au navigateur du visiteur connecte.
    }
  }); // Instruction d'execution.

  // Routes Mur de Graffitis // Instruction d'execution.
  server.on("/api/messages", HTTP_GET, handleGetGraffitis); // Enregistrement d'un point d'acces d'API Web (URL de routage).
  server.on("/api/messages", HTTP_POST, handlePostGraffiti); // Enregistrement d'un point d'acces d'API Web (URL de routage).
  server.on("/api/remote_graffiti", HTTP_POST, handleRemoteGraffiti); // Enregistrement d'un point d'acces d'API Web (URL de routage).
  // Nouvelles Routes Civvi // Instruction d'execution.
  server.on("/api/sync-time", HTTP_POST, handleTimeSync); // Enregistrement d'un point d'acces d'API Web (URL de routage).
  server.on("/synchro-temps", HTTP_POST, handleTimeSync); // Enregistrement d'un point d'acces d'API Web (URL de routage).
  server.on("/api/vote", HTTP_POST, handlePostVote); // Enregistrement d'un point d'acces d'API Web (URL de routage).
  server.on("/api/notifications", HTTP_GET, handleGetNotifications); // Enregistrement d'un point d'acces d'API Web (URL de routage).



  server.on("/api/journal", HTTP_GET, handleGetJournal); // Enregistrement d'un point d'acces d'API Web (URL de routage).
  server.on("/api/clear_journal", HTTP_POST, handleClearJournal); // Enregistrement d'un point d'acces d'API Web (URL de routage).
  server.on("/api/clear_journal", HTTP_DELETE, handleClearJournal); // Enregistrement d'un point d'acces d'API Web (URL de routage).
  server.on("/api/delete_graffiti", HTTP_POST, handleDeleteGraffiti); // Enregistrement d'un point d'acces d'API Web (URL de routage).
  server.on("/api/history", HTTP_GET, handleGetHistory); // Enregistrement d'un point d'acces d'API Web (URL de routage).
  server.on("/api/clear_history", HTTP_POST, handleClearHistory); // Enregistrement d'un point d'acces d'API Web (URL de routage).

  server.onNotFound(handleNotFound); // Enregistrement d'un point d'acces d'API Web (URL de routage).

  server.begin(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  Serial.println("Serveur prêt. Connectez-vous et rendez-vous sur 192.168.4.1"); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
}

// ===== BOUCLE PRINCIPALE ===== // Affectation de valeur a une variable ou modification d'etat.
void loop() { // Boucle principale d'execution du programme ESP32 repetee indefiniment.
  server.handleClient(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.

  if (!cvRequestsQueue.empty()) { // Verification d'une condition logique.
    CVRequest req = cvRequestsQueue.back(); // Affectation de valeur a une variable ou modification d'etat.
    cvRequestsQueue.pop_back(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    envoyerCV(req.targetMac, req.requesterMac); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
  }

  if (fonctionnementMode == 0) { // Only in Relais Mode // Verification d'une condition logique.
    // Nettoyage radar (efface après 3 minutes) // Appel d'une fonction interne ou d'une memoire de bibliotheque.
    bool fsAChange = false; // Affectation de valeur a une variable ou modification d'etat.
    for (int i = 0; i < messages.size(); i++) { // Execution d'une boucle iterative.
      if (millis() - messages[i].dernierContact > TIMEOUT_MS) { // Verification d'une condition logique.
        messages.erase(messages.begin() + i); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
        i--; // Instruction d'execution.
        fsAChange = true; // Affectation de valeur a une variable ou modification d'etat.
      }
    }

    if (fsAChange || resauvegarder) { // Verification d'une condition logique.
      sauvegarderPensees(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      resauvegarder = false; // Affectation de valeur a une variable ou modification d'etat.
    }

    for (auto it = messagesEnAttente.begin(); it != messagesEnAttente.end();) { // Execution d'une boucle iterative.
      if (millis() - it->second.dernierUpdate > 10000) { // Verification d'une condition logique.
        it = messagesEnAttente.erase(it); // Affectation de valeur a une variable ou modification d'etat.
      } else { // Instruction d'execution.
        ++it; // Instruction d'execution.
      }
    }

    for (auto it = cvsEnAttente.begin(); it != cvsEnAttente.end();) { // Execution d'une boucle iterative.
      if (millis() - it->second.dernierUpdate > 10000) { // Verification d'une condition logique.
        it = cvsEnAttente.erase(it); // Affectation de valeur a une variable ou modification d'etat.
      } else { // Instruction d'execution.
        ++it; // Instruction d'execution.
      }
    }

    if (millis() - dernierEnvoi > 10000) { // Verification d'une condition logique.
      envoyerTexteLong(); // Appel d'une fonction interne ou d'une memoire de bibliotheque.
      dernierEnvoi = millis(); // Affectation de valeur a une variable ou modification d'etat.
    }
  }
}
