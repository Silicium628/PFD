#ifndef FG_DATA_H
#define FG_DATA_H

/*
Liste des balises VOR et ILS

RAMARQUE : Ce fichier est commun aux codes sources FG_panel et FG_radio, il ne doit figurer qu'une fois sur le disque
la seconde occurence necessaire pour la compilation peut se faire par un lien symbolique (sous Linux),
ou par placement judicieux dans un dossier partagé par le système Arduino (comme c'est le cas par exemple pour les lib graphiques).

Quoi qu'il en soit, la même version doit être compilée dans les deux programmes,
afin de rendre les affichages cohérents entre les deux côtés, ainsi que leur utilisation.

Tout ceci dans le but d'alléger les données échangées par WiFi entre les deux montages, et donc le temps de traitement.
Seul le numéro de l'aérodrome choisi sera transmis.
*/

/*
fait:
-Annecy

todo:
-Barcelone ***
-Béziers-Cap-d'Agde
-Brest *
-Calais
-Chambéry ****
-Cherbourg
-Geneve ***
-Istres-Le-Tube
-La Rochelle
-Lannion
-Londres
-Lyon
-Nantes
-Naple
-Nimes-Garons
-Palma
-Perpignan-Rivesaltes
-Pise
-Reykjavik
-Rouen
-Salon-de-Provence
-St Nazaire
-St-Malo
-Venise
*/


struct FGBAL
{
	char ID_OACI[4+1];
	char nom[18+1];
	char ID_VOR[3+1];
	char ID_ILS[3+1];
	uint8_t typ; // ILS (Instrument Landing System), LOC (Localizer), GS (Glide  Slope), DME (distance) - codage sur un seul octet
	uint16_t frq_VOR;
	uint16_t frq_ILS;
	uint16_t orient_ILS; // orientation de la piste
	
};

// mettre à jour la ligne suivante pour tout ajout d'aérodrome
const uint16_t nb_elements = 13; // ATTENTION: erreur non permise ici sinon plantage assuré; compter le "n=0" !

//ATTENTION : pour toute modif de ce fichier, il faut recompiler ET reprogrammer le ND et le PFD afin qu'ils exploitent bien les même données
// Le ND envoie le n° de balise en cours au PFD (par WiFi) qui en déduit la fréquence radio 
// de la balise (par lecture du code de ce fichier inclus) et l'envoie à Flightgear par l'USB
// donc la transmission ND->PFD c'est juste un nombre, qui permet ensuite au PFD de connaitre toutes les caractéristiques de l'aérodrome.
// ce nombre est transmis sous la forme d'un argument lors d'une requête html.
// Voir le requête "void httpGetHDG()" du ND, et la fonction "setup()" du PFD


struct FGBAL liste_bali[nb_elements]; 

char n_bal_array[4]; // 3 char + zero terminal - pour envoi par WiFi


uint8_t _LOC = 0b00000001;	// LOCALIZER
uint8_t _DME = 0b00000010;	// DISTANCE MEASURING EQUIPMENT	
uint8_t _GS  = 0b00000100;	// GLIDE
uint8_t _NDB = 0b00001000;  // Non-directional beacon (Balise non directionnelle)



// source des données qui suivent :

// https://flightplandatabase.com/
// https://flightplandatabase.com/tools
// https://flightplandatabase.com/airport/LFBO

// sur les conseils de :
// http://fr.flightgear.org/forums/viewtopic.php?id=4714

// pour les VOR :
// https://skyvector.com/

void init_FG_bali()
{
	uint16_t n;

	n=0;
	strcpy(liste_bali[n].ID_OACI, "LFKJ");
	strcpy(liste_bali[n].nom, "AJACCIO"); // "NAPOLEON BONAPARTE"
	strcpy(liste_bali[n].ID_VOR, "AJO");
	liste_bali[n].typ=_LOC+_GS;
	liste_bali[n].frq_VOR = 11480;
	strcpy(liste_bali[n].ID_ILS, "AC");
	liste_bali[n].frq_ILS = 11030;
	liste_bali[n].orient_ILS = 25;  // piste 02 - direction  25°N pointant vers la piste (et vers la montagne !!!)
	// piste 02 -> OK pour approche et attérissage ILS
	// décollage plutôt piste 20 (la même mais vers la mer)
	// décollage piste 02 poossible avec le Citation X mais faire une 180 à gauche de suite ! 

	n=1;
	strcpy(liste_bali[n].ID_OACI, "LFLP");
	strcpy(liste_bali[n].nom, "ANNECY"); // "ANNECY MEYTHET" Altitude 1521m
	strcpy(liste_bali[n].ID_VOR, "CBY"); // CHAMBERY-ALPES NOR
	liste_bali[n].typ=_LOC+_GS;
	liste_bali[n].frq_VOR = 11540;
	strcpy(liste_bali[n].ID_ILS, "ANM");
	liste_bali[n].frq_ILS = 10890;
	liste_bali[n].orient_ILS = 37;  // RWY 04 = 37° ;  RWY 22 = 217°


	n=2;
	strcpy(liste_bali[n].ID_OACI, "LFMV");
	strcpy(liste_bali[n].nom, "AVIGNON Caumont");
	strcpy(liste_bali[n].ID_VOR, "AVN");
	liste_bali[n].typ=_DME+_NDB;  // apparemment pas de glide suite à interférences avec celui d'Orange !
	liste_bali[n].frq_VOR = 11460;
	strcpy(liste_bali[n].ID_ILS, "CM");
	liste_bali[n].frq_ILS = 11050;
	liste_bali[n].orient_ILS = 168;

	n=3;
	strcpy(liste_bali[n].ID_OACI, "LFKB");
	strcpy(liste_bali[n].nom, "BASTIA PORETTA");
	strcpy(liste_bali[n].ID_VOR, "BTA");
	liste_bali[n].typ=_LOC+_GS;
	liste_bali[n].frq_VOR = 11415;
	strcpy(liste_bali[n].ID_ILS, "");
	liste_bali[n].frq_ILS = 11135;
	liste_bali[n].orient_ILS = 349;

	n=4;
	strcpy(liste_bali[n].ID_OACI, "LFBD");
	strcpy(liste_bali[n].nom, "BORDEAUX MERIGNAC");
	strcpy(liste_bali[n].ID_VOR, "BMC");
	liste_bali[n].typ=_LOC+_GS;
	liste_bali[n].frq_VOR = 11375;
	strcpy(liste_bali[n].ID_ILS, "BEI");
	liste_bali[n].frq_ILS = 11115;
	liste_bali[n].orient_ILS = 286; 


	n=5;
	strcpy(liste_bali[n].ID_OACI, "LFKC");
	strcpy(liste_bali[n].nom, "CALVI"); //  St.Catherine
	strcpy(liste_bali[n].ID_VOR, "CV"); // VOR au nord-nord-est de l'aérodrome
	liste_bali[n].typ=_LOC+_DME; // pas de glide !
	liste_bali[n].frq_VOR = 10950;
	strcpy(liste_bali[n].ID_ILS, "CLI");
	liste_bali[n].frq_ILS = 10950; // piste 18
	liste_bali[n].orient_ILS = 179;

	
	n=6;
	strcpy(liste_bali[n].ID_OACI, "LFLC");
	strcpy(liste_bali[n].nom, "CLERMONT FERRAND");
	strcpy(liste_bali[n].ID_VOR, "CFA");
	liste_bali[n].typ=_LOC+_GS;
	liste_bali[n].frq_VOR = 11435;
	strcpy(liste_bali[n].ID_ILS, "CF");
	liste_bali[n].frq_ILS = 11110;
	liste_bali[n].orient_ILS = 262;


	n=7;
	strcpy(liste_bali[n].ID_OACI, "LFKF");
	strcpy(liste_bali[n].nom, "FIGARI SUD CORSE");
	strcpy(liste_bali[n].ID_VOR, "FGI");
	liste_bali[n].typ=_LOC+_GS;
	liste_bali[n].frq_VOR = 11670;
	strcpy(liste_bali[n].ID_ILS, "GR");
	liste_bali[n].frq_ILS = 11050;
	liste_bali[n].orient_ILS = 227;

	n=8;
	strcpy(liste_bali[n].ID_OACI, "LFML");	
	strcpy(liste_bali[n].nom, "MARSEILLE PROVENCE");
	strcpy(liste_bali[n].ID_VOR, "MRM");
	liste_bali[n].typ=_LOC+_GS;
	liste_bali[n].frq_VOR = 10880;
	strcpy(liste_bali[n].ID_ILS, "MPV"); 
	liste_bali[n].frq_ILS = 11115; 
	liste_bali[n].orient_ILS = 314; // piste 32 

	n=9;
	strcpy(liste_bali[n].ID_OACI, "LFMT");
	strcpy(liste_bali[n].nom, "MONTPELLIER"); 
	strcpy(liste_bali[n].ID_VOR, "FRJ");
	liste_bali[n].typ=_LOC+_GS;
	liste_bali[n].frq_VOR = 11445;
	strcpy(liste_bali[n].ID_ILS, "FG");
	liste_bali[n].frq_ILS = 10855;
	liste_bali[n].orient_ILS = 305;	// piste 31R

	n=10;
	strcpy(liste_bali[n].ID_OACI, "LFMN");
	strcpy(liste_bali[n].nom, "NICE COTE D'AZUR");
	strcpy(liste_bali[n].ID_VOR, "CGS");
	liste_bali[n].typ=_LOC+_GS;
	liste_bali[n].frq_VOR = 10920;
	strcpy(liste_bali[n].ID_ILS, "NI");
	liste_bali[n].frq_ILS = 11070;  // piste 04R
	liste_bali[n].orient_ILS = 45;

	n=11;
	strcpy(liste_bali[n].ID_OACI, "LFPG");
	strcpy(liste_bali[n].nom, "PARIS Ch-De-Gaulle"); 
	strcpy(liste_bali[n].ID_VOR, "PGS");
	liste_bali[n].typ=_LOC+_GS;
	liste_bali[n].frq_VOR = 11705;  // à voir
	strcpy(liste_bali[n].ID_ILS, "DSU");
	liste_bali[n].frq_ILS = 10835;
	liste_bali[n].orient_ILS = 265; // piste 26L  (orientée 86°<->266°, donc la plus au sud)


	n=12;
	strcpy(liste_bali[n].ID_OACI, "LFBO");
	strcpy(liste_bali[n].nom, "TOULOUSE BLAGNAC"); 
	strcpy(liste_bali[n].ID_VOR, "TOU");
	liste_bali[n].typ=_LOC+_GS;
	liste_bali[n].frq_VOR = 11770;
	strcpy(liste_bali[n].ID_ILS, "TG");
	liste_bali[n].frq_ILS = 10890; // piste 14L
	liste_bali[n].orient_ILS = 143;

}


#endif
