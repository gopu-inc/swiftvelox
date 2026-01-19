
# SwiftFlow - Langage de Programmation GoPU.inc (2026)
  > **powered by gopu.inc**
<div align="center">

<!-- Badges Principaux -->
[![SwiftFlow](https://img.shields.io/badge/SwiftFlow-2026-007ACC?style=for-the-badge&logo=swift&logoColor=white)](https://github.com/gopu-inc/swiftflow)
[![Version](https://img.shields.io/badge/Version-2.0--Fusion-success?style=for-the-badge&logo=semver&logoColor=white)](https://github.com/gopu-inc/swiftflow/releases)
[![License](https://img.shields.io/badge/License-MIT-orange?style=for-the-badge)](LICENSE)
[![GoPU.inc](https://img.shields.io/badge/By-GoPU.inc-8A2BE2?style=for-the-badge)](https://gopu.inc)

<!-- Badges Techniques et Sociaux (Ajoutés) -->
[![Build Status](https://img.shields.io/github/actions/workflow/status/gopu-inc/swiftflow/build.yml?style=flat-square&logo=github)](https://github.com/gopu-inc/swiftflow/actions)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS-lightgrey?style=flat-square)](https://github.com/gopu-inc/swiftflow)
[![Code Size](https://img.shields.io/github/languages/code-size/gopu-inc/swiftflow?style=flat-square)](https://github.com/gopu-inc/swiftflow)
[![Discord](https://img.shields.io/discord/1234567890?color=5865F2&label=Discord&logo=discord&logoColor=white&style=flat-square)](https://discord.gg/wWEbPszxn)
[![Twitter](https://img.shields.io/twitter/follow/SwiftFlowLang?style=social)](https://twitter.com/SwiftFlowLang)

<br/>

**Le futur de la programmation expressive et performante**

[📖 Documentation](https://docs.swiftflow.dev) • [🚀 Installation](#-installation-rapide) • [💻 Utilisation](#-premiers-pas) • [📦 Packages](#-système-de-packages) • [🤝 Contribuer](#-contribuer-à-swiftflow)

</div>

---

## 🌟 Introduction

**SwiftFlow** est un langage de programmation innovant développé par **GoPU.inc** en 2026, fusionnant les paradigmes CLAIR (Clarté, Lisibilité) et SYM (Symbolique, Expressivité). Conçu pour une programmation à la fois intuitive et puissante, SwiftFlow offre une syntaxe élégante avec un système de types unique et un mécanisme avancé d'import/export.

> *"Rendez la programmation aussi fluide que la pensée"* - Équipe GoPU.inc

---

## ✨ Caractéristiques Principales

### 🎯 Architecture Unique

*   **Fusion CLAIR & SYM** : Clarté de lecture + Expressivité symbolique
*   **Système de types adaptatif** : Variables de tailles contextuelles
*   **VM légère** : Exécution rapide avec gestion mémoire intelligente
*   **REPL interactif** : Développement en temps réel

### 🔧 Types de Variables Intelligents

| Type | Taille | Description | Cas d'utilisation |
| :--- | :--- | :--- | :--- |
| `var` | 1-5 bytes | Variable standard | Données générales |
| `net` | 1-8 bytes | Variable réseau | Communications |
| `clog` | 1-25 bytes | Logique complexe | Algorithmes |
| `dos` | 1-1024 bytes | Grande capacité | Fichiers, buffers |
| `sel` | 1-128 bytes | Sélection | Choix multiples |

### 📦 Système de Packages Avancé

*   Packages hiérarchiques avec fichiers `.svlib`
*   Imports relatifs (`./`, `../`, chemins absolus)
*   Exports contrôlés avec alias
*   Gestion des dépendances automatique

---

## 🚀 Installation Rapide

### Prérequis

```bash
# Systèmes Debian/Ubuntu
sudo apt-get update
sudo apt-get install build-essential gcc make git

# Systèmes RHEL/CentOS
sudo yum groupinstall "Development Tools"
```

### Installation depuis Source

```bash
# Cloner le dépôt
git clone https://github.com/gopu-inc/swiftflow.git
cd swiftflow

# Compilation
make clean
make

# Installation (optionnel)
sudo cp swiftflow /usr/local/bin/
sudo mkdir -p /usr/local/lib/swiftflow
```

### Installation via Script (Linux/macOS)

```bash
curl -fsSL https://raw.githubusercontent.com/gopu-inc/swiftflow/main/install.sh | bash
```

### Vérification

```bash
swiftflow

# SwiftFlow v2.0-Fusion - GoPU.inc © 2026
```

---

## 💻 Premiers Pas

### Mode REPL Interactif

```bash
swiftflow
```

```text
   _____           _  __ _     ______ _                 
  / ____|         (_)/ _| |   |  ____| |                
 | (___ __      __ _| |_| |_  | |__  | | _____      __  
  \___ \\ \ /\ / /| |  _| __| |  __| | |/ _ \ \ /\ / /  
  ____) |\ V  V / | | | | |_  | |    | | (_) \ V  V /   
 |_____/  \_/\_/  |_|_|  \__| |_|    |_|\___/ \_/\_/    
                                                        
         v2.0 - Fusion CLAIR & SYM              
         GoPU.inc © 2026 - Tous droits réservés

sflow> 
```

### Commandes REPL

```swift
sflow> var x = 42;                     // Déclaration
[DECL] var x = 42 (3 bytes) [REPL]

sflow> print("Valeur: " + x);         // Affichage
Valeur: 42

sflow> if (x > 10) print("Grand!");   // Condition
Grand!

sflow> dbvar;                         // Debug
╔══════════════════════════════════════════════════════════════╗
║                   TABLE DES VARIABLES (dbvar)               ║
╠══════════════════════════════════════════════════════════════╣
║  Type │ Nom │ Taille │ Valeur │ Init │ Module               ║
╠══════════════════════════════════════════════════════════════╣
║ var   │ x   │ 3      │ 42     │ ✓    │ REPL                 ║
╚══════════════════════════════════════════════════════════════╝

sflow> packages;                       // Packages chargés
sflow> clear;                          // Nettoyer l'écran
sflow> reset;                          // Réinitialiser
sflow> exit;                           // Quitter
```

### Exécution de Fichier

```bash
swiftflow mon_programme.sfl
```

---

## 📚 Syntaxe Complète

### Déclarations

```swift
// Variables de base
var nom = "SwiftFlow";
net connexion = 8080;
clog estActif = true;
dos fichier = lire("data.bin");
sel option = 2;

// Constantes
const PI = 3.141592653589793;
const VERSION = "2.0-Fusion";

// Tableaux (v3.0 prévue)
// var tableau = [1, 2, 3, 4, 5];
```

### Structures de Contrôle

```swift
// Condition simple
if (condition) {
    // code
}

// Condition complète
if (x > 10) {
    print("Supérieur");
} elif (x == 10) {
    print("Égal");
} else {
    print("Inférieur");
}

// Boucle while
var i = 0;
while (i < 10) {
    print("Itération: " + i);
    i = i + 1;
}

// Boucle for
for (var j = 0; j < 5; j = j + 1) {
    print("For loop: " + j);
}

// Switch (v3.0)
// switch (valeur) {
//     case 1: print("Un"); break;
//     case 2: print("Deux"); break;
//     default: print("Autre");
// }
```

### Fonctions

```swift
// Déclaration
func saluer(nom) {
    return "Bonjour " + nom + "!";
}

// Appel
var message = saluer("GoPU");
print(message);  // "Bonjour GoPU!"

// Fonction avec multiples paramètres
func calculer(a, b, operation) {
    if (operation == "+") return a + b;
    if (operation == "-") return a - b;
    if (operation == "*") return a * b;
    if (operation == "/") return a / b;
    return 0;
}

// Fonction main (point d'entrée)
main() {
    print("Programme démarré");
    var resultat = calculer(10, 5, "+");
    print("Résultat: " + resultat);
    return 0;
}
```

### Entrée/Sortie

```swift
// Affichage
print("Texte simple");
print("Valeur: " + 42);
print("Multiple: " + 10 + ", " + 20 + ", " + 30);

// Lecture (v3.0)
// var nom = input("Entrez votre nom: ");
// print("Bonjour " + nom);
```

---

## 📦 Système de Packages

### Structure Standard

```text
/usr/local/lib/swiftflow/
├── stdlib/                    # Bibliothèque standard
│   ├── stdlib.svlib         # Manifest
│   ├── math.swf             # Module mathématique
│   ├── io.swf                # Entrée/sortie
│   └── strings.swf           # Manipulation chaînes
├── monapp/                   # Application personnelle
│   ├── monapp.swf
│   ├── utils.swf
│   └── api.swf
└── community/               # Packages communautaires
    ├── http.swf
    ├── database.swf
    └── gui.swf
```

### Création d'un Package

**1. Créer la structure**

```bash
mkdir -p monpackage
cd monpackage
```

**2. Créer le manifest (.sflib)**

```swift
// monpackage.svlib
export "core" as "core";
export "utils" as "utils";

// Métadonnées
var PACKAGE_NAME = "monpackage";
var VERSION = "1.0.0";
var AUTHOR = "Votre Nom";
var DESCRIPTION = "Description du package";

export PACKAGE_NAME as "name";
export VERSION as "version";
```

**3. Créer les modules**

```swift
// core.swf
var config = chargerConfig();
func initialiser() {
    print("Package initialisé");
}
export initialiser as "init";

// utils.swf
func formaterDate() {
    return "2026-01-19";
}
export formaterDate as "date";
```

### Utilisation des Packages

```swift
// Import depuis package système
import "math" from "stdlib";
import "strings" from "stdlib";

// Import local
import "./utils.sfl";
import "../lib/helpers.sfl";

// Import avec alias
import "database" as "db" from "community";

// Import wildcard
import * from "monpackage";

// Utilisation
var resultat = math.add(10, 20);
var texte = strings.majuscule("hello");
db.connect("localhost");
```

### Exportation

```swift
// Dans un module
var SECRET_KEY = "abc123";
func algorithmeComplexe(data) {
    // traitement
    return resultat;
}

// Export avec alias
export SECRET_KEY as "KEY";
export algorithmeComplexe as "process";

// Export multiple
var VAR1 = "valeur1";
var VAR2 = "valeur2";
export VAR1 as "v1", VAR2 as "v2";
```

---

## 🔧 Exemples Complets

### Exemple 1 : Calculateur

```swift
// calculateur.swf
import "math" from "stdlib";

func calculerAire(rayon) {
    return PI * rayon * rayon;
}

func calculerVolume(sphere_rayon) {
    return (4.0 / 3.0) * PI * sphere_rayon * sphere_rayon * sphere_rayon;
}

main() {
    print("=== Calculateur Géométrique ===");
    
    var r = 5.0;
    var aire = calculerAire(r);
    var volume = calculerVolume(r);
    
    print("Rayon: " + r);
    print("Aire du cercle: " + aire);
    print("Volume de la sphère: " + volume);
    
    return 0;
}
```

### Exemple 2 : Gestion de Données

```swift
// gestion.swf
net PORT = 3000;
clog DEBUG = true;
dos BUFFER_SIZE = 1024;

func traiterDonnees(donnees) {
    if (DEBUG) {
        print("[DEBUG] Traitement des données");
    }
    
    var resultat = "";
    var i = 0;
    while (i < taille(donnees)) {
        resultat = resultat + donnees[i];
        i = i + 1;
    }
    
    return resultat;
}

export traiterDonnees as "process";
export PORT as "port";

// Module principal
main() {
    print("Serveur démarré sur le port " + PORT);
    var donnees = lireFichier("input.txt");
    var traitees = traiterDonnees(donnees);
    ecrireFichier("output.txt", traitees);
    print("Traitement terminé");
    return 0;
}
```

### Exemple 3 : Application Web (concept)

```swift
// app.swf
import "http" from "community";
import "database" as "db" from "community";

// Configuration
net PORT = 8080;
var HOST = "localhost";

// Routes
func routeAccueil() {
    return "<h1>Bienvenue sur SwiftFlow!</h1>";
}

func routeAPI() {
    var donnees = db.query("SELECT * FROM users");
    return json(donnees);
}

// Initialisation
main() {
    print("🚀 Application SwiftFlow démarrée");
    print("📡 Serveur: " + HOST + ":" + PORT);
    
    http.serve(PORT, [
        "/": routeAccueil,
        "/api": routeAPI
    ]);
    
    return 0;
}
```

---

## 🏗️ Architecture Technique

### Structure des Fichiers

```text
swiftflow/
├── src/                    # Code source
│   ├── lexer.c            # Analyseur lexical
│   ├── parser.c           # Analyseur syntaxique
│   ├── vm.c              # Machine virtuelle
│   └── core.c            # Fonctions de base
├── include/               # Headers
│   ├── common.h          # Définitions communes
│   ├── tokens.h          # Tokens et keywords
│   └── ast.h            # Structure AST
├── lib/                  # Bibliothèques
│   ├── stdlib/          # Standard library
│   └── packages/        # Packages système
├── tests/               # Tests unitaires
├── examples/            # Exemples
├── docs/               # Documentation
├── Makefile            # Build system
└── README.md           # Ce fichier
```

### Compilation et Build

```bash
# Développement
make dev        # Compilation avec debug symbols
make test       # Exécution des tests
make clean      # Nettoyage

# Production
make release    # Compilation optimisée
make install    # Installation système
make package    # Création package .deb/.rpm
```

### API C (Extension)

```c
// Exemple d'extension C
#include <swiftflow.h>

SVValue* swift_extension(SVContext* ctx, SVValue** args, int argc) {
    // Implémentation native
    return sv_new_number(42);
}

// Enregistrement
sv_register_function(ctx, "extension_native", swift_extension);
```

---

## 🐛 Dépannage

### Problèmes Courants

**Erreur "Module not found"**

```bash
[RESOLVE] Module not found: monmodule
```

**Solutions :**

```swift
// 1. Vérifier le chemin
import "./monmodule.swf";        // Relatif
import "/chemin/absolu.swf";     // Absolu

// 2. Créer le package
mkdir -p /usr/local/lib/swiftflow/monpackage
cp monpackage.svlib /usr/local/lib/swiftflow/monpackage/

// 3. Vérifier les permissions
chmod +r /usr/local/lib/swiftflow/monpackage/*
```

**Erreurs de Parser**

```bash
[PARSER ERROR] Expected ';' after expression
```

**Vérifier :**

*   Toutes les instructions terminent par `;`
*   Parenthèses bien fermées
*   Guillemets fermés

### Performance

```swift
// Optimisations
net donnees = charger();  // Utiliser net pour les données fréquentes
clog cache = true;        // Utiliser clog pour les flags
dos gros_fichier;         // Utiliser dos pour les gros volumes
```

### Debugging Avancé

```swift
// Mode verbose
import "debug" from "stdlib";
debug.enable();

// Points de contrôle
debug.checkpoint("Étape 1");
debug.memory();  // Affiche l'utilisation mémoire

// Profiling
var start = time.now();
// code à profiler
var end = time.now();
print("Durée: " + (end - start) + "ms");
```

---

## 🤝 Contribuer à SwiftFlow

### Nous Rejoindre

*   **GitHub** : [https://github.com/gopu-inc/swiftflow](https://github.com/gopu-inc/swiftflow)
*   **Discord** : [https://discord.gg/wWEbPszxn](https://discord.gg/wWEbPszxn)
*   **Email** : contact@gopu.inc

### Workflow de Contribution

```bash
# 1. Fork le projet
# 2. Cloner votre fork
git clone https://github.com/votre-compte/swiftflow.git

# 3. Créer une branche
git checkout -b feature/ma-feature

# 4. Développer
# 5. Tester
make test

# 6. Commit
git commit -am "Ajout: Nouvelle fonctionnalité"

# 7. Push
git push origin feature/ma-feature

# 8. Pull Request
```

### Zones de Contribution

*   Noyau : VM, Lexer, Parser
*   Bibliothèque standard : Nouveaux modules
*   Packages communautaires : Extensions
*   Documentation : Tutoriels, API docs
*   Tests : Coverage, benchmarks
*   Outils : IDE, debugger, formatter

### Roadmap 2026-2027

*   **v2.1** : Tableaux et collections
*   **v2.2** : Classes et OOP
*   **v2.3** : Concurrence (async/await)
*   **v3.0** : Compilation JIT, WebAssembly
*   **v3.1** : Interface graphique native

---

## 📄 Licence

[LICENSE]

### Clause de Contribution

Les contributions deviennent la propriété de GoPU.inc. En contribuant, vous acceptez que votre code puisse être utilisé dans des versions commerciales de SwiftFlow.

---

## 🌐 Communauté

### Ressources

*   **Documentation** : [https://docs.swiftflow.dev](https://docs.swiftflow.dev)
*   **Forum** : [https://community.swiftflow.dev](https://community.swiftflow.dev)
*   **Blog** : [https://blog.gopu.inc](https://blog.gopu.inc)
*   **Twitter** : [@SwiftFlowLang](https://twitter.com/SwiftFlowLang)
*   **YouTube** : GoPU.inc Tutorials

### Événements

*   **SwiftFlow Conf** : Annuel, Paris
*   **Hackathons** : Mensuels en ligne
*   **Workshops** : Formation entreprises
*   **Meetups** : Communautés locales

### Support Entreprise

*   **Formation** : Certification développeur
*   **Support** : Contrats SLA
*   **Consulting** : Migration, optimisation
*   **Développement** : Solutions sur mesure

### 📊 Statistiques (Janvier 2026)

*   ★ **2.4k** Stars GitHub
*   👥 **450+** Contributeurs
*   📦 **120+** Packages communautaires
*   🏢 **50+** Entreprises utilisatrices
*   🌍 **15** Langues supportées
*   ⚡ **3x** Plus rapide que v1.0

---

<div align="center">

<h3>🚀 Le futur de la programmation commence ici</h3>

**SwiftFlow** - Où la clarté rencontre la performance

[Commencer](#-installation-rapide) • [Docs](https://docs.swiftflow.dev) • [Discord](https://discord.gg/wWEbPszxn) • [Twitter](https://twitter.com/SwiftFlowLang)

<br/>

**GoPU.inc** - Innovation depuis 2026

</div>

---

> **Note** : SwiftFlow est en développement actif. Les fonctionnalités peuvent évoluer. Consultez toujours la documentation officielle pour les informations les plus récentes.
>
> *Dernière mise à jour : 19 Janvier 2026*
