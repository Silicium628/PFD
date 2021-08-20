/*
	PFD.ino  - Primary Flight Display pour Flightgear et ESP32 - version Afficheur TFT 480x320

	par Silicium628

	Ce logiciel est libre et gratuit sous licence GNU GPL
	
*/

/**---------------------------------------------------------------------------------------	
	Pour les #includes issus de l'univers Arduino (que je ne fournis pas), il faut voir au cas par cas.
	(drivers d'affichage en particulier)

---------------------------------------------------------------------------------------
	De petites images à placer sur la SDcard centrées sur les aérodromes proviennent de OpenStreetMap
	OpenStreetMap® est un ensemble de données ouvertes,
	disponibles sous la licence libre Open Data Commons Open Database License (ODbL)
	accordée par la Fondation OpenStreetMap (OSMF).

	Voir ce lien pour plus de détails :
	https://www.openstreetmap.org/copyright
--------------------------------------------------------------------------------------**/

/*=====================================================================================================
CONCERNANT L'AFFICHAGE TFT : connexion :

( Pensez à configurer le fichier User_Setup.h de la bibliothèque ~/Arduino/libraries/TFT_eSPI/ )

les lignes qui suivent ne sont q'un commentaire pour vous indiquer la config à utiliser
placée ici, elle ne sont pas fonctionnelles
Il FAUT modifier le fichier User_Setup.h installé par le système Arduino dans ~/Arduino/libraries/TFT_eSPI/

// ESP32 pins used for the parallel interface TFT
#define TFT_CS   27  // Chip select control pin
#define TFT_DC   14  // Data Command control pin - must use a pin in the range 0-31
#define TFT_RST  26  // Reset pin

#define TFT_WR   12  // Write strobe control pin - must use a pin in the range 0-31
#define TFT_RD   13

#define TFT_D0   16  // Must use pins in the range 0-31 for the data bus
#define TFT_D1   4  // so a single register write sets/clears all bits
#define TFT_D2   2  // 23
#define TFT_D3   22
#define TFT_D4   21
#define TFT_D5   15 // 19
#define TFT_D6   25 // 18
#define TFT_D7   17
=====================================================================================================

Notes :

- Nous choisissons Nav1 pour le VOR et Nav2 pour l'ILS
- Si les data de FG ne sont pas reçues, il faut vérifier que le PFD est bien connecté sur le port USB0 (et pas USB1 ou autre...)
*/

	String version="16.0"; // fonction Autoland + Wifi Dialogue ND
	
// la fonction Autoland() utilise le glide pour la descente et non plus une altitude calculée théorique
// v15.2 modif du fichier hardware4.xml (nav-distance)
// v15.3 revu autoland - possibilité de poser auto si localizer seul (aérodrome non équipé de glide de glide). modifs gestion voyants
// v15.6 nombreuses modifs dans tous les fichiers, dont fichier FG_data.h
// v16.0 prise en charge du module "SW" (boutons poussoirs) par WiFi
	
// le numéro de version doit être identique à celui du ND (Navigation Display)


#include "PFD.h"
#include <stdint.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include "Free_Fonts.h"

#include "FS.h"
#include "SD.h"
#include "SPI.h"

#include "FG_data.h"

#include <WiFi.h>
#include "ESPAsyncWebServer.h"


const char* ssid = "PFD_srv"; 
const char* password = "72r4TsJ28";

AsyncWebServer server(80); // Create AsyncWebServer object on port 80

String argument_recu1;

// TFT_eSPI TFT = TFT_eSPI(); // Configurer le fichier User_Setup.h de la bibliothèque TFT_eSPI au préalable

TFT_eSprite SPR_E = TFT_eSprite(&TFT480);  // Declare Sprite object "SPR_11" with pointer to "TFT" object
TFT_eSprite SPR_N = TFT_eSprite(&TFT480);
TFT_eSprite SPR_O = TFT_eSprite(&TFT480);
TFT_eSprite SPR_S = TFT_eSprite(&TFT480);

VOYANT voyant_L; // Localizer
VOYANT voyant_G; // Glide


//uint16_t s_width = TFT480.Get_Display_Width();  
//uint16_t s_heigh = TFT480.Get_Display_Height();

int16_t Ax_actu, Ay_actu; 
int16_t Bx_actu, By_actu;


// COULEURS RGB565
// Outil de sélection -> http://www.barth-dev.de/online/rgb565-color-picker/

#define NOIR   		0x0000
#define ROUGE  		0xF800
#define ROSE		0xFBDD
#define ORANGE 		0xFBC0
#define JAUNE   	0xFFE0
#define JAUNE_PALE  0xF7F4
#define VERT   		0x07E0
#define VERT_FONCE  0x02E2
#define OLIVE		0x05A3
#define CYAN    	0x07FF
#define BLEU_CLAIR  0x455F
#define AZUR  		0x1BF9
#define BLEU   		0x001F
#define MAGENTA 	0xF81F
#define VIOLET1		0x781A
#define VIOLET2		0xECBE
#define GRIS_TRES_CLAIR 0xDEFB
#define GRIS_CLAIR  0xA534
#define GRIS   		0x8410
#define GRIS_FONCE  0x5ACB
#define GRIS_TRES_FONCE  0x2124

#define GRIS_AF 	0x51C5		// 0x3985
#define BLANC   	0xFFFF


// AVIONIQUE : Horizon Artificiel (HA):

#define HA_CIEL		0x33FE
#define HA_SOL		0xAA81 //0xDB60

//position et dimensions de l'horizon artificiel

#define HA_x0 210
#define HA_y0 130
#define HA_w 120 // demi largeur
#define HA_h 100	// demi hauteur

#define x_autopilot 320

// Width and height of sprite
#define SPR_W 14
#define SPR_H 14


/*
#define taille1 100000  //480*320*2
uint8_t data_ecran[taille1];
*/

uint32_t memo_micros = 0;
uint32_t temps_ecoule;
uint16_t nb_secondes=0;

float roulis;
float tangage;

int32_t altitude; // accepte les valeurs négatives (par exemple si QNH mal réglé avant décollage)
int32_t gnd_elv;
int32_t vitesse;
int32_t memo_vitesse;
int16_t dV;
int16_t acceleration;
int16_t vspeed;		// vitesse verticale

int16_t cap; 		// en degrés d'angle; direction actuelle du nez de l'avion


int16_t hdg1  = 150; // en degrés d'angle; consigne cap = Heading (HDG) Bug
int16_t memo_hdg1;
uint8_t refresh_hdg=0;
uint8_t traitement_switches=0;

int16_t asel1 = 30; // consigne altitude 30 -> 3000ft

char var_array[5];	// 4 char + zero terminal - pour envoi par WiFi

// pour VOR (sur Nav1 libellé "nav" dans FG)
int32_t vor_dst; // en mètres
uint32_t vor_frq;
float vor_actual; // 0..35900
uint16_t vor_actual_deg; // 0..359 

// pour ILS (sur Nav2 libellé "nav[1]" dans FG)
int32_t ils_dst;
float ils_nm;
uint32_t ils_frq;
float ils_loc; 		// angle de déviation (erreur / axe de la piste) dans le plan horizontal
float ils_glide; 	// angle de descente (pente) vue depuis la balise
float ils_radial;	// orientation de la piste
float ils_actual;	// direction de l'avion du de la piste
float ils_actual_deg;  // 0..359

int16_t loc=0; 		// localizer
int16_t memo_loc=0;

int16_t gld=0; 		// glide
int16_t memo_gld=0;

int16_t num_bali=0; 

uint8_t SDcardOk=0;
uint16_t data_ok;
uint8_t gs_ok=0;
uint8_t QNH_ok=0;

uint8_t premier_passage =1;
uint8_t au_sol=1;
uint8_t attente_data=1;

uint8_t autoland=0;

//uint8_t take_off_ok =0; // passe à 1 si altitude atteinte = 3000ft


float memo_R2;
int16_t memo_y0;

const int bouton1 = 36;  // attention: le GPIO 36 n'a pas de R de pullup interne, il faut en câbler une au +3V3
bool bouton1_etat;
bool memo_bouton1_etat;

String switches; // boutons connectés au 3eme ESP32, reçus par WiFi
uint16_t v_switches=0;
uint16_t memo_v_switches=0;

// deux encodeurs rotatifs pas à pas
const int rot1a = 32;  // GPIO32 -> câbler une R au +3V3
const int rot1b = 33;  // GPIO33 -> câbler une R au +3V3

const int rot2a = 35;  // GPIO35 -> câbler une R au +3V3
const int rot2b = 34;  // GPIO34 -> câbler une R au +3V3

//const int led1 = 25; // GPIO15


#define TEMPO 5 // tempo anti-rebond pour l'acquisition des encodeurs rotatifs
volatile uint32_t timer1 = 0;
volatile uint32_t timer2 = 0;

uint16_t compteur1;

// TFT480.setTextColor(TFT_BLACK, TFT_WHITE);
// TFT480.drawLine(x0, y0, x1, y1, TFT_BLACK);
// TFT480.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_GREEN);
// TFT480.drawRect(5, 3, 230, 119, TFT_BLACK);
// TFT480.fillRect(5, 3, 230, 119, TFT_WHITE);


void RAZ_variables()
{
	roulis=0;
	tangage=0;
	altitude=0;
	gnd_elv=0;
	vitesse=0;
	vspeed=0;		
	cap=0; 		
	memo_hdg1=0;
	vor_dst=0;
	vor_actual=0;
	vor_actual_deg=0;
	ils_dst=0;
	ils_loc=0; 		
	ils_glide=0; 	
	loc=0;
	memo_loc=0;
	gld=0;
	memo_gld=0;
}



/** ***********************************************************************************
		IMAGE.bmp					
***************************************************************************************/

/**
Rappel et décryptage de la fonction Color_To_565 : (elle se trouve dans le fichier LCDWIKI_KBV.cpp)

//Pass 8-bit (each) R,G,B, get back 16-bit packed color

uint16_t Color_To_565(uint8_t r, uint8_t g, uint8_t b)
{
	return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
}

0xF8 = 11111000
0xFC = 11111100

(r & 0xF8) -> 5 bit de gauche de r (on ignore donc les 3 bits de poids faible)
(g & 0xFC) -> 6 bit de gauche de g (on ignore donc les 2 bits de poids faible)
(b & 0xF8) -> 5 bit de gauche de b (on ignore donc les 3 bits de poids faible)

rrrrr---
gggggg--
bbbbb---

après les décallages on obtient les 16 bits suivants:

rrrrr---========
     gggggg--===
        ===bbbbb

soit après le ou :

rrrrrggggggbbbbb       

calcul de la Fonction inverse : 
RGB565_to_888
**/


void RGB565_to_888(uint16_t color565, uint8_t *R, uint8_t *G, uint8_t *B)
{
	*R=(color565 & 0xFFFFF800) >> 8;
	*G=(color565 & 0x7E0) >> 3;
	*B=(color565 & 0x1F) << 3 ;
}



/** -----------------------------------------------------------------------------------
		CAPTURE D'ECRAN vers SDcard				
/** ----------------------------------------------------------------------------------- */

void write_TFT_on_SDcard() // enregistre le fichier .bmp
{
	if (SDcardOk==0) {return;}

	String s1;
	uint16_t ys=200;
	TFT480.setFreeFont(FF1);
	TFT480.setTextColor(JAUNE, NOIR);
	
	uint16_t x, y;
	uint16_t color565;
	uint16_t bmp_color;
	uint8_t R, G, B;
	
	if( ! SD.exists("/bmp/capture2.bmp"))
	{
		TFT480.fillRect(0, 0, 480, 320, NOIR); // efface
		TFT480.setTextColor(ROUGE, NOIR);
		TFT480.drawString("NO /bmp/capture2.bmp !", 100, ys);
		delay(300);
		TFT480.fillRect(100, ys, 220, 20, NOIR); // efface
		return;
	}


	File File1 = SD.open("/bmp/capture2.bmp", FILE_WRITE); // ouverture du fichier binaire (vierge) en écriture
	if (File1)
	{
/*
Les images en couleurs réelles BMP888 utilisent 24 bits par pixel: 
Il faut 3 octets pour coder chaque pixel, en respectant l'ordre de l'alternance bleu, vert et rouge.
*/
		uint16_t bmp_offset = 138;
		File1.seek(bmp_offset);

		
		TFT480.setTextColor(VERT, NOIR);;	
		
		for (y=320; y>0; y--)
		{
			for (x=0; x<480; x++)
			{
				color565=TFT480.readPixel(x, y);

				RGB565_to_888(color565, &R, &G, &B);
				
				File1.write(B); //G
				File1.write(G); //R
				File1.write(R); //B
			}

			s1=(String) (y/10);
			TFT480.fillRect(450, 300, 20, 20, NOIR);
			TFT480.drawString(s1, 450, 300);// affi compte à rebour
		} 

		File1.close();  // referme le fichier
		
		TFT480.fillRect(450, 300, 20, 20, NOIR); // efface le compte à rebour
	}
}

/** ----------------------------------------------------------------------------------- */




void Draw_arc_elliptique(uint16_t x0, uint16_t y0, int16_t dx, int16_t dy, float alpha1, float alpha2, uint16_t couleur) // alpha1 et alpha2 en radians
{
/*
REMARQUES :	
-cette fonction permet également de dessiner un arc de cercle (si dx=dy), voire le cercle complet
- dx et dy sont du type int (et pas uint) et peuvent êtres négafifs, ou nuls.
-alpha1 et alpha2 sont les angles (en radians) des caps des extrémités de l'arc 
*/ 
	uint16_t n;
	float i;
	float x,y;

	i=alpha1;
	while(i<alpha2)
	{
		x=x0+dx*cos(i);
		y=y0+dy*cos(i+M_PI/2.0);
		TFT480.drawPixel(x,y, couleur);
		i+=0.01;  // radians
	}
}


void affi_rayon1(uint16_t x0, uint16_t y0, uint16_t rayon, double angle, float pourcent, uint16_t couleur_i, bool gras)
{
// trace une portion de rayon de cercle depuis 100%...à pourcent du rayon du cercle
// angle en radians - sens trigo
	float x1, x2;
	float y1, y2;

	x1=x0+rayon* cos(angle);
	y1=y0-rayon* sin(angle);

	x2=x0+pourcent*rayon* cos(angle);
	y2=y0-pourcent*rayon* sin(angle);

	TFT480.drawLine(x1, y1, x2, y2, couleur_i);
	if (gras)
	{
		TFT480.drawLine(x1, y1-1, x2, y2-1, couleur_i);
		TFT480.drawLine(x1, y1+1, x2, y2+1, couleur_i);
	}
}



void affi_rayon2(uint16_t x0, uint16_t y0, float r1, float r2, float angle_i, uint16_t couleur_i, bool gras)
{
// trace une portion de rayon de cercle entre les distances r1 et r2 du centre
// angle_i en degrés décimaux - sens trigo

	float angle =angle_i/57.3;  // (57.3 ~ 180/pi)
	int16_t x1, x2;
	int16_t y1, y2;

	x1=x0+int16_t(r1* cos(angle));
	y1=y0-int16_t(r1* sin(angle));

	x2=x0+int16_t(r2* cos(angle));
	y2=y0-int16_t(r2* sin(angle));

	if ((x1>0) && (x2>0) && (y1>0) && (y2>0) && (x1<480) && (x2<480) && (y1<320) && (y2<320) )
	{
		TFT480.drawLine(x1, y1, x2, y2, couleur_i);
		
		if (gras)
		{
			TFT480.drawLine(x1, y1-1, x2, y2-1, couleur_i);
			TFT480.drawLine(x1, y1+1, x2, y2+1, couleur_i);
		}
	}

	//TFT480.fillCircle(x2, y2, 2, ROUGE);
}




void affi_tiret_H(uint16_t x0, uint16_t y0, uint16_t r, double angle_i, uint16_t couleur_i)
{
// trace un tiret perpendiculaire à un rayon de cercle de rayon r
// angle_i en degrés décimaux

	float angle =angle_i/57.3;  // (57.3 ~ 180/pi)
	float x1, x2;
	float y1, y2;

	x1=x0+(r)* cos(angle-1);
	y1=y0-(r)* sin(angle-1);		

	x2=x0+(r)* cos(angle+1);
	y2=y0-(r)* sin(angle+1);

	TFT480.drawLine(x1, y1,  x2, y2, couleur_i);	
}



void affi_pointe(uint16_t x0, uint16_t y0, uint16_t r, double angle_i, uint16_t couleur_i)
{
// trace une pointe de flèche sur un cercle de rayon r
// angle_i en degrés décimaux - sens trigo

	float angle =angle_i/57.3;  // (57.3 ~ 180/pi)
	int16_t x1, x2, x3;
	int16_t y1, y2, y3;

	x1=x0+r* cos(angle); // pointe
	y1=y0-r* sin(angle); // pointe

	x2=x0+(r-7)* cos(angle-0.1); // base A
	y2=y0-(r-7)* sin(angle-0.1); // base A		

	x3=x0+(r-7)* cos(angle+0.1); // base B
	y3=y0-(r-7)* sin(angle+0.1); // base B

	TFT480.fillTriangle(x1, y1,  x2, y2,  x3, y3,	couleur_i);	
}


void affi_rectangle_incline(uint16_t x0, uint16_t y0, uint16_t r, double angle_i, uint16_t couleur_i)
{
//rectangle inscrit dans le cerce de rayon r
// angle_i en degrés décimaux - sens trigo

	float angle =angle_i/57.3;  // (57.3 ~ 180/pi)
	int16_t x1, x2, x3, x4;
	int16_t y1, y2, y3, y4;
	float d_alpha=0.08; // détermine la largeur du rectangle

// point 1
	x1=x0+r*cos(angle-d_alpha); 
	y1=y0+r*sin(angle-d_alpha);	
// point 2
	x2=x0+r*cos(angle+d_alpha); 
	y2=y0+r*sin(angle+d_alpha);	
// point 3
	x3=x0+r*cos(M_PI + angle-d_alpha); 
	y3=y0+r*sin(M_PI + angle-d_alpha);
// point 4
	x4=x0+r*cos(M_PI + angle+d_alpha); 
	y4=y0+r*sin(M_PI + angle+d_alpha);

	TFT480.drawLine(x1, y1,  x2, y2, couleur_i);
	TFT480.drawLine(x2, y2,  x3, y3, couleur_i);
	TFT480.drawLine(x3, y3,  x4, y4, couleur_i);
	TFT480.drawLine(x4, y4,  x1, y1, couleur_i);
		
}



void affi_indexH(uint16_t x, uint16_t y, int8_t sens, uint16_t couleur) 
{
// petite pointe de flèche horizontale
// sens = +1 ou -1 pour orienter la pointe vers la droite ou vers la gauche

	TFT480.fillTriangle(x, y-4,  x, y+4,  x+8*sens, y, couleur);
}



void affi_indexV(uint16_t x, uint16_t y, int8_t sens, uint16_t couleur) 
{
// petite pointe de flèche verticale
// sens = +1 ou -1 pour orienter la pointe vers le haut ou vers le bas

	TFT480.fillTriangle(x-4, y,  x+4, y,  x, y+8*sens, couleur);
}



float degTOrad(float angle)
{
	return (angle * M_PI / 180.0);
}



void init_affi_HA()
{
	TFT480.fillRect(HA_x0-HA_w, HA_y0-HA_h-1, 2*HA_w, HA_h+1, HA_CIEL);
	TFT480.fillRect(HA_x0-HA_w, HA_y0-HA_h +  HA_h, 2*HA_w, HA_h, HA_SOL);

}




void dessine_avion()
{
// aile gauche	
	TFT480.fillRect(HA_x0-102, HA_y0-3, 60, 10, BLANC);	//H contour en blanc
	TFT480.fillRect(HA_x0-42, HA_y0-3, 10, 19, BLANC);	//V
	
	TFT480.fillRect(HA_x0-100, HA_y0-1, 60, 5, NOIR);	//H
	TFT480.fillRect(HA_x0-40, HA_y0-1, 5, 15, NOIR);		//V


// aile droite	
	TFT480.fillRect(HA_x0+28, HA_y0-3, 64, 10, BLANC);	//H contour en blanc
	TFT480.fillRect(HA_x0+28, HA_y0-3, 10, 19, BLANC);	//V
	
	TFT480.fillRect(HA_x0+30, HA_y0-1, 60, 5, NOIR);		//H
	TFT480.fillRect(HA_x0+30, HA_y0-1, 5, 15, NOIR);		//V

//carré blanc au centre

	TFT480.fillRect(HA_x0-4, HA_y0-3, 8, 2, BLANC);
	TFT480.fillRect(HA_x0-4, HA_y0-3, 2, 8, BLANC);

	TFT480.fillRect(HA_x0-4, HA_y0+3, 10, 2, BLANC);
	TFT480.fillRect(HA_x0+4, HA_y0-3, 2, 8, BLANC);

	//affi_dst_NAV();

}



void lign_sep(uint16_t Ax, uint16_t Ay, uint16_t Bx, uint16_t By)
{
// actualise la ligne de séparation ciel-sol

	TFT480.drawLine(Ax, Ay-1, Bx, By-1, HA_CIEL);
	TFT480.drawLine(Ax, Ay, Bx, By, BLANC);
	TFT480.drawLine(Ax, Ay+1, Bx, By+1, HA_SOL);
}



void arrondissement_coins()
{

// fillTriangle(int32_t x1,int32_t y1, int32_t x2,int32_t y2, int32_t x3,int32_t y3, uint32_t color);

//HG
	TFT480.fillTriangle(
		HA_x0-HA_w, HA_y0-HA_h-1,
		HA_x0-HA_w, HA_y0-HA_h+20,
		HA_x0-HA_w+20, HA_y0-HA_h-1,
	NOIR);


//----------------------------------------------
//HD
	TFT480.fillTriangle(
		HA_x0+HA_w, HA_y0-HA_h-1,
		HA_x0+HA_w, HA_y0-HA_h+20,
		HA_x0+HA_w-20, HA_y0-HA_h-1,
	NOIR);

	

//----------------------------------------------
//BG
	TFT480.fillTriangle(
		HA_x0-HA_w, HA_y0+HA_h+1,
		HA_x0-HA_w, HA_y0+HA_h-20,
		HA_x0-HA_w+20, HA_y0+HA_h+1,
	NOIR);

//----------------------------------------------
//BD
	TFT480.fillTriangle(
		HA_x0+HA_w, HA_y0+HA_h+1,
		HA_x0+HA_w, HA_y0+HA_h-20,
		HA_x0+HA_w-20, HA_y0+HA_h+1,
	NOIR);

}



void affi_HA()  // Horizon Artificiel
{
	String s1;
	
	////String s1=(String) roulis;
	////TFT480.drawString(s1, 400, 20);


// pivot
	int16_t x0=0;
	int16_t y0=0;

//points d'intersection avec le bord du carré
	int16_t Ax, Ay; // sur le bord gauche
	int16_t Bx, By; // sur le bord droit
// Le dessin consistera à tracer des segments colorés entre les points A et B	

	
// roulis -> [-90..+90]

// normalisation de la valeur R2 -> toujours >0
	float R2 = -1*roulis;
	if (R2<0) {R2+=360;} // ce qui est un angle identique, de valeur positive (sens trigo)

// le pivot reste centré horizontalement mais se déplace verticalement en fonction du tangage	
	y0 += 2*tangage;

//calcul & memorisation de ces deux facteurs, ce qui évitera 2 calculs de tangente à chaque passage dan la fonction
	float tgt_moins = tan(degTOrad(90-R2));
	float tgt_plus = tan(degTOrad(90+R2));
	
//-----------------------------------------------------------------------------
// CALCUL COTE DROIT (point B)

// calcul du point B d'intersection 
	Bx=HA_w;
	By=y0 + HA_w*tan(degTOrad(R2));

//test si le point d'intersection se trouve plus haut que le haut du carré :
	
	if(By>HA_h)
	{
		By=HA_h;
		Bx = x0 + (HA_h-y0)*tgt_moins;
	}
	
	if(By< -HA_h)
	{
		By= -HA_h;
		Bx = x0 + (HA_h+y0)*tgt_plus;
	}
//-----------------------------------------------------------------------------
// CALCUL COTE GAUCHE (point A)

	Ax=-HA_w;
	Ay=y0 - HA_w*tan(degTOrad(R2));

	if(Ay> HA_h)
	{
		Ay= HA_h;
		Ax = x0 + (HA_h-y0)*tgt_moins;
	}

	if(Ay< -HA_h)
	{
		Ay= -HA_h;
		Ax = x0 + (HA_h+y0)*tgt_plus;
	}
	
	
//-----------------------------------------------------------------------------
// positionnement de l'ensemble sur l'écran	

	Ax += HA_x0;
	Ay += HA_y0;

	Bx += HA_x0;
	By += HA_y0;

// pour éviter un tracé hors cadre au premier passage :
	if (premier_passage == 1)
	{
		Ax_actu = Ax;
		Ay_actu = Ay;
				
		Bx_actu = Bx;
		By_actu = By;
		premier_passage=0;
	}


//-----------------------------------------------------------------------------
// ligne "verticale" d'inclinaison (tangage)

	affi_rayon2(HA_x0, HA_y0, 85, -memo_y0, 90-memo_R2, HA_CIEL, false); // efface partie supérieure
	affi_rayon2(HA_x0, HA_y0, 85, -y0, 90-R2, BLANC, false);	// retrace ligne partie supérieure
	
	affi_rayon2(HA_x0, HA_y0, -85,-memo_y0, 90-memo_R2, HA_SOL, false);  // efface partie inférieure
	affi_rayon2(HA_x0, HA_y0, -85,-y0, 90-R2, VERT, false);	// retrace ligne partie inférieure

	affi_pointe(HA_x0, HA_y0, 85, 90-memo_R2, HA_CIEL); // efface
	affi_pointe(HA_x0, HA_y0, 85, 90-R2, BLANC); // retrace


	affi_glide();
	affi_localizer();

// graduation fixe
	TFT480.setFreeFont(FF1);
	TFT480.setTextColor(BLANC, GRIS_AF);
	TFT480.drawString("30", HA_x0-70, HA_y0-98);
	TFT480.drawString("60", HA_x0-120, HA_y0-55);
	
	TFT480.drawString("30", HA_x0+60, HA_y0-98);
	TFT480.drawString("60", HA_x0+100, HA_y0-55);

//-----------------------------------------------------------------------------
// animation de la ligne de séparation horizontale

	while ((Bx_actu != Bx) ||  (By_actu != By) || (Ax_actu != Ax) ||  (Ay_actu != Ay))
	{
	// déplacements successifs de 1 pixel de chaque extrémités de la ligne	

		//TFT480.drawLine(Bx, By, x2, y2, BLANC);

		if (Bx_actu < Bx) { Bx_actu++; lign_sep(Ax_actu, Ay_actu, Bx_actu, By_actu); }
		if (Bx_actu > Bx) { Bx_actu--; lign_sep(Ax_actu, Ay_actu, Bx_actu, By_actu); }
				
		if (Ax_actu < Ax) { Ax_actu++; lign_sep(Ax_actu, Ay_actu, Bx_actu, By_actu); }
		if (Ax_actu > Ax) { Ax_actu--; lign_sep(Ax_actu, Ay_actu, Bx_actu, By_actu); }
		
		if (By_actu < By) { By_actu++; lign_sep(Ax_actu, Ay_actu, Bx_actu, By_actu); }
		if (By_actu > By) { By_actu--; lign_sep(Ax_actu, Ay_actu, Bx_actu, By_actu); }
		
		if (Ay_actu < Ay) { Ay_actu++; lign_sep(Ax_actu, Ay_actu, Bx_actu, By_actu); }
		if (Ay_actu > Ay) { Ay_actu--; lign_sep(Ax_actu, Ay_actu, Bx_actu, By_actu); }

	}
	

// graduation roulis qui se déplace angulairement avec la ligne de tangage
	for (int8_t n=0; n<4; n++)
	{
		Draw_arc_elliptique(HA_x0, HA_y0, 20*n, 20*n, degTOrad(80-memo_R2+n), degTOrad(100-memo_R2-n), HA_CIEL); // efface bas
		Draw_arc_elliptique(HA_x0, HA_y0, 20*n, 20*n, degTOrad(80-R2+n), degTOrad(100-R2-n), BLANC); // trace bas

		Draw_arc_elliptique(HA_x0, HA_y0, 20*n, 20*n, degTOrad(260-memo_R2+n), degTOrad(280-memo_R2-n), HA_SOL); // efface haut
		Draw_arc_elliptique(HA_x0, HA_y0, 20*n, 20*n, degTOrad(260-R2+n), degTOrad(280-R2-n), BLANC); // trace haut
	}
	
	memo_R2 = R2;
	memo_y0 = y0;
	


	
//-----------------------------------------------------------------------------
	arrondissement_coins();

	//affi_frq_NAV1();
	affi_dst_NAV1();
	affi_dst_NAV2();
	
	// affi_alti_NAV2();
}



void affi_acceleration()
{
// POUR TEST **********
////String s2= (String) acceleration;
////TFT480.fillRect(100, 50, 200, 20, TFT_BLACK);
////TFT480.setFreeFont(FF5);
////TFT480.setTextColor(BLANC, NOIR);
////TFT480.drawString("Acceleration=", 100, 50);
////TFT480.drawString(s2, 250, 50);
// ******************** 
	
//barres verticales colorées juste à droite de la vitesse indiquant sa variation
	uint16_t x0=60;
	uint16_t Y_zero=162;
	
	int16_t dy=0;
	
//fleche haute
	TFT480.fillRect(x0, 30, 8, Y_zero, GRIS_TRES_FONCE); // efface haut
	if (acceleration > 1)
	{
		dy= acceleration;
		
		TFT480.fillRect(x0, Y_zero-dy, 8, dy, VERT); // fleche 
	}


//fleche basse
	TFT480.fillRect(x0, Y_zero, 8, 132, GRIS_TRES_FONCE); // efface bas
	if (acceleration < -1)
	{
		dy= -acceleration;
		
		TFT480.fillRect(x0, Y_zero, 8, dy, JAUNE); // fleche
	}
	
	TFT480.fillRect(x0, Y_zero, 10, 2, BLANC);

	TFT480.fillRect(x0, 290, 8, 20, NOIR);
	
}


void bride(int16_t *valeur)
{
	int16_t y_min =30;
	int16_t y_max =290;	
	if (*valeur<y_min) {*valeur=y_min;}
	if (*valeur>y_max) {*valeur=y_max;}
}


void affi_switches()
{
	TFT480.setTextFont(1);
	TFT480.setTextColor(JAUNE);
	TFT480.fillRect(420, 0, 50, 10, NOIR); // efface le nombre précédemment affiché 
	TFT480.drawString(switches, 420, 0);
}



void affi_vitesse()
{
	uint16_t x1;
	String s1;
	
	int16_t y_min =30;
	int16_t y_max =290;	

	TFT480.setTextColor(BLANC); // Background is not defined so it is transparent
	
//---------------------------------------------------------------------------------------
//bande verticale multicolore

	#define vitesse_sol 40
	int16_t vitesse_mini1 =90;
	int16_t vitesse_mini2 =130;
	int16_t vitesse_maxi1 =200;
	int16_t vitesse_maxi2 =280;



//calcul de la position des limites	entre les différentes couleurs verticales

	int16_t d1, d2, d3, d4, d5;

	d1=(int16_t)(HA_y0+HA_w/2 + 3.2*((vitesse - vitesse_sol)));		
	d2=(int16_t)(HA_y0+HA_w/2 + 3.2*((vitesse - vitesse_mini1)));	
	d3=(int16_t)(HA_y0+HA_w/2 + 3.2*((vitesse - vitesse_mini2)));	
	d4=(int16_t)(HA_y0+HA_w/2 + 3.2*((vitesse - vitesse_maxi1)));	
	d5=(int16_t)(HA_y0+HA_w/2 + 3.2*((vitesse - vitesse_maxi2)));	

	bride(&d1);
	bride(&d2);
	bride(&d3);
	bride(&d4);
	bride(&d5);
	
	int16_t h1, h2, h3, h4, h5;
	
	h1 = y_max-(int16_t)d1;
	h2 = d1-d2;	
	h3 = d2-d3;	
	h4 = d3-d4;	
	h5 = d4-d5;
	
	TFT480.fillRect(50, 30, 6, (int16_t)d5,  ORANGE);
	TFT480.fillRect(50, d5, 6, h5, JAUNE); 
	TFT480.fillRect(50, d4, 6, h4, VERT); 
	TFT480.fillRect(50, d3, 6, h3, ORANGE);
	TFT480.fillRect(50, d2, 6, h2, ROUGE);
	TFT480.fillRect(50, d1, 6, 290-(int16_t)d1, GRIS);

	TFT480.fillRect(50, 290, 6, 20, NOIR);

//---------------------------------------------------------------------------------------
//échelle verticale graduée glissante

	uint16_t y0;
	
	int16_t vit1;
	float d6;
	
	TFT480.setFreeFont(FF1);

	y0=3.2*(vitesse%10);

	TFT480.fillRect(0, y_min, 50, y_max-30, GRIS_AF); // bande verticale à gauche
	for(int n=0; n<10; n++)
	{
		d6 =2+y0+32.0*n; // 24 pixels verticalement entre chaque trait -> 10*24 = 240px (hauteur de l'affiur)
		{
			if ( (d6>y_min) && (d6<y_max-10) && (vit1>=0) && (vit1<1000) ) 
			{
				TFT480.fillRect(45, (int16_t)d6, 10, 2, BLANC); // petits tirets horizontaux
			}

			vit1 = vitesse -10*(n-5);
			vit1 /= 10;
			vit1 *= 10;
			s1=(String) vit1;

			if ( (d6>y_min) && (d6<y_max-10) && (vit1>=0) && (vit1<1000) ) 
			{
				TFT480.setTextColor(BLANC, GRIS_AF);
				//TFT480.drawString("    ", 9, d6);
				x1=0; 
				if(vit1<100){x1=7;} // pour affichage centré
				if(vit1<10){x1=14;}
				if (vit1>=10)
				{
					TFT480.drawString(s1, x1, (uint16_t)d6-5); // Graduation (tous les 20kts)
				}
			}
		}
	}
	//TFT480.fillRect(0, 0, 65, 20, NOIR);
	
//---------------------------------------------------------------------------------------
// affichage de la valeur principale

	uint16_t VP_y0 = 150;

	TFT480.setTextColor(BLANC, NOIR);
	TFT480.setFreeFont(FF18);
	s1=(String)vitesse;
	
	TFT480.fillRect(3, VP_y0, 42, 26, NOIR); // efface le nombre précédemment affiché (pour le cas où on passe de trois à deux chiffres)
	
	if ((vitesse>=0) && (vitesse <1000))
	{
		x1=3; 
		if(vitesse<100){x1=10;} // pour affichage centré
		if(vitesse<10){x1=20;}
		TFT480.drawString(s1, x1, VP_y0+3); // affi le nombre
	} // affi en gros à mi-hauteur de l'écran
	else
	{ TFT480.fillRect(3, VP_y0, 42, 26, GRIS); }

	TFT480.drawRoundRect(1, VP_y0-1, 45, 28, 5, BLANC); // encadrement de la valeur centrale affichée

	TFT480.fillTriangle(45, VP_y0+7, 45, VP_y0+17, 55, VP_y0+12, NOIR); // petit triangle (curseur) noir
}



void affi_asel()
{
// consigne ALTITUDE de l'autopilot
// ( chiffres en rose en haut à droite	)

	if(asel1 >=0)
	{
		String s2 =(String)(asel1*100);     
		TFT480.setTextColor(ROSE, NOIR);
		TFT480.setFreeFont(FF5);
		uint16_t x1;
		x1=340;
		TFT480.fillRect(x1+7, 0, 70, 20, NOIR); // efface
		if(asel1<10000){x1+=7;}
		if(asel1<1000){x1+=7;} 
		if(asel1<100){x1+=7;} // pour affichage centré
		if(asel1<10){x1+=7;}
		
		TFT480.drawString(s2, x1, 5);	
	}
}



void affi_vt_verticale()
{
// affichage analogique sur la droite de l'écran

	uint16_t x0=435;
	uint16_t y0=165;

	float y1;

	uint16_t x1;
	String s1;
	
	TFT480.fillRect(x0, y0-90, 45, 180, GRIS_AF); // barre grise
	TFT480.fillRect(x0, y0, 25, 2, BLEU_CLAIR);  // centre

// ------------------------
// graduations sur un arc vertical
	TFT480.setFreeFont(FF1);

	TFT480.setTextColor(BLANC, NOIR);
	TFT480.drawString("ft/mn", x0-8, y0+125);


	TFT480.setTextColor(BLANC, GRIS_AF);



	float angle;
	for(uint8_t n=0; n<7; n++ )
	{
		angle =135+ n*15;  // 1 tiret tous les 15 degrés
		affi_rayon1(HA_x0+340, y0, 110, degTOrad(angle), 0.9, BLANC, false); // tirets de graduation
	}


	TFT480.drawString("3", x0+9, y0-90);
	TFT480.drawString("2", x0-3, y0-65);
	TFT480.drawString("1", x0-8, y0-35);
	TFT480.drawString("0", x0-3, y0-5 + 0);
	TFT480.drawString("1", x0-8, y0+25);
	TFT480.drawString("2", x0-3, y0+50);
	TFT480.drawString("3", x0+9, y0+75);
	
// ------------------------
// aiguille à droite de l'écran
	float angle2;
	
	TFT480.setFreeFont(FF1);
	s1=(String) (vspeed*60);
	
	angle2 = 180.0 - vspeed *0.92;

	TFT480.fillRect(x0-10, y0-110, 55, 15, GRIS_TRES_FONCE); // efface haut
	TFT480.fillRect(x0-10, y0+105, 55, 15, GRIS_TRES_FONCE); // efface bas

	if ((vspeed > -50) && (vspeed < 50))
	{
		affi_rayon1(HA_x0+330, y0, 100, degTOrad(angle2), 0.7, JAUNE, true);
		
		TFT480.setTextColor(JAUNE, NOIR);
	}
	else if (vspeed > 50)
	{
		affi_rayon1(HA_x0+330, y0, 110, degTOrad(132), 0.7, JAUNE, true);
		TFT480.setTextColor(JAUNE, NOIR);
		
		TFT480.drawString(s1, x0-10, y0-110);
	}
	else if (vspeed < -50)
	{
		affi_rayon1(HA_x0+330, y0, 110, degTOrad(228), 0.7, JAUNE, true);
		TFT480.setTextColor(JAUNE, NOIR);
		
		TFT480.drawString(s1, x0-10, y0+105);
	}


// affichage digital de la valeur
	

/*	
// = vitesse ascensionnelle, sous forme de barres verticales vertes, à droite, près de l'echelle d'altitude	
	uint16_t x0=405;
	uint16_t y0=40;
	
	int16_t dy=0;
	
//fleche haute
	TFT480.fillRect(x0, 0, 10, 140, GRIS_FONCE); // efface haut
	if (vspeed > 1)
	{
		dy= vspeed;
		
		TFT480.fillRect(x0, y0+100-dy, 10, dy, VERT); // fleche 
	}
	

//fleche basse
	TFT480.fillRect(x0, y0+150, 10, 135, GRIS_FONCE); // efface bas
	if (vspeed < -1)
	{
		dy= -vspeed;
		
		TFT480.fillRect(x0, y0+150, 10, dy, VERT); // fleche
	}
*/
	
}




void affi_cap()
{
// cercle tournant de CAP gradué en bas au centre de l'écran
// Les lettres 'N' 'S' 'E' 'O' pour Nord Sud Est Ouset sont initialisées sous forme de sprites dans la fonction setup()

	uint16_t x02 = 200;
	uint16_t y02 = 350;
	float angle; // en radians
	//float cap_RD; // en radians (le cap fourni par FG étant en degrés d'angle)
	uint16_t x_spr;
	uint16_t y_spr;

	uint16_t x_hdg;
	uint16_t y_hdg;

	uint8_t R =70;
	uint8_t R2 =R-6;
/**
360° =2 pi rad
1° = 2 pi/360 rad = pi/180 rad
**/

	TFT480.fillCircle(x02,y02, R, GRIS_AF);

	for(uint8_t n=0; n<24; n++ )
	{
		angle = cap+15 + n*15;  // 1 tiret tous les 15 degrés
		affi_rayon1(x02, y02, (R-5), degTOrad(angle), 0.9, BLANC, false); // tirets de graduation
	}
	x_hdg =  x02 + R2*cos(degTOrad(hdg1-90-cap));
	y_hdg =  y02 + R2*sin(degTOrad(hdg1-90-cap));
	
	TFT480.drawLine(x02, y02, x_hdg, y_hdg, VERT);
	TFT480.drawCircle(x_hdg, y_hdg, 5, VERT); // rond vert sur le cercle = consigne de cap	de l'autopilot

	x_spr = x02+R2 * cos(degTOrad(angle));
	y_spr = y02-R2 * sin(degTOrad(angle));
	TFT480.setPivot(x_spr, y_spr);
	SPR_E.pushRotated(-cap+90, TFT_BLACK); // Plot rotated Sprite, black = transparent

	x_spr = x02+R2* cos(degTOrad(angle+90));
	y_spr = y02-R2 * sin(degTOrad(angle+90));
	TFT480.setPivot(x_spr, y_spr);
	SPR_N.pushRotated(-cap, TFT_BLACK); 

	x_spr = x02+R2 * cos(degTOrad(angle+180));
	y_spr = y02-R2 * sin(degTOrad(angle+180));
	TFT480.setPivot(x_spr, y_spr);
	SPR_O.pushRotated(-cap-90, TFT_BLACK); 

	x_spr = x02+R2 * cos(degTOrad(angle-90));
	y_spr = y02-R2 * sin(degTOrad(angle-90));
	TFT480.setPivot(x_spr, y_spr);
	SPR_S.pushRotated(-cap, TFT_BLACK); 
	

// petite "maison" dans le cercle (valeur du cap)

	#define a 180   // x général
	#define b a+20
	#define c b+20
	#define d 288	// y général
	#define e d+10
	#define f e+20

	TFT480.drawLine(a, f, c, f, BLANC); // sol
	TFT480.drawLine(a, f, a, e, BLANC); // mur de gauche
	TFT480.drawLine(c, f, c, e, BLANC); // mur de droite
	TFT480.drawLine(a, e, b, d, BLANC); // toit pente gauche
	TFT480.drawLine(c, e, b, d, BLANC); // toit pente droite


// affi la valeur	
	String s1;
	uint16_t x0 = a+2;
	uint16_t y0 = e;
	
	uint16_t x1= x0;
	if(cap<100){x1+=5;} // pour affichage centré
	if(cap<10){x1+=5;}

	s1=(String) cap;

	TFT480.fillRect(x0, y0, 35, 20, NOIR); // efface le nombre précédemment affiché
	TFT480.setTextColor(BLANC, NOIR);
	TFT480.setFreeFont(FM9);
	TFT480.drawString(s1, x1, y0);

	
}



void affi_altitude()
{
	String s1;
	uint16_t x0 =365;
//---------------------------------------------------------------------------------------
//échelle verticale graduée glissante

	uint16_t x1;
	uint16_t y0;	
	int16_t alt1;
	float d5;
	
	TFT480.setFreeFont(FF1);

	y0=3.2*(altitude%10);

	TFT480.fillRect(x0, 20, 60, 319, GRIS_AF); //efface bande verticale à droite

	
	for(int n=0; n<10; n++)
	{
		d5 =0+y0+32.0*n; // pixels verticalement entre chaque trait -> 10*24 = 240px (hauteur de l'affi)
		{
			if (d5>=20)  // marge en haut
			{
				TFT480.fillRect(x0, (int16_t)d5+5, 5, 2, BLANC); // petits tirets horizontaux

				alt1 = altitude -10*(n-5);
				alt1 /= 10;
				alt1 *= 10;
				s1=(String) alt1;

				if(alt1>=0)
				{
					TFT480.setTextColor(BLANC, GRIS_AF);
					//TFT480.drawString("    ", 9, d5);
					x1=x0;
					if(alt1<10000){x1+=7;} // pour affichage centré
					if(alt1<1000){x1+=7;} 
					if(alt1<100){x1+=7;} 
					if(alt1<10){x1+=7;}
					TFT480.drawString(s1, x1, (uint16_t)d5); // Graduation (tous les 20kts)
				}
			}
		}
	}


//---------------------------------------------------------------------------------------
// affichage de la valeur principale
	
	uint16_t x2;
	uint16_t y0b = 155;
	TFT480.fillRect(x0-20, y0b, 80, 25, NOIR); // efface le nombre précédemment affiché
	TFT480.setTextColor(BLANC, NOIR);
	TFT480.setFreeFont(FF18);
	
	if ((1) && (altitude < 60000))
	{
		s1=(String) altitude;
	}
	else {s1="----";}
	x2=x0-20;
	if(altitude<10000){x2+=10;} // pour affichage centré
	if(altitude<1000){x2+=10;}
	if(altitude<100){x2+=10;} 
	if(altitude<10){x2+=10;}
	
	if(altitude<0)
	{
		TFT480.setTextColor(ROUGE);
		x2=x0-20; // si valeur négative affichée avec signe "-"
	} 
	
	
	TFT480.drawString(s1, x2, y0b);
	TFT480.drawRoundRect(x0-20, y0b-3, 75, 28, 5, BLANC); // encadrement de la valeur centrale affichée


	if ((altitude <0) && (au_sol==1))
	{
		QNH_ok =0;
		TFT480.setFreeFont(FM9);
		TFT480.fillRect(150, 180, 145, 30, ROUGE);
		TFT480.setTextColor(BLANC);
		TFT480.drawString("Altitude < 0", 150, 180);
		TFT480.drawString("reglez le QNH", 150, 195); // calage altimétrique requis
/*
	voir ce lien à propos du QNH :	- https://wiki.flightgear.org/Fr/Altitude
	
	Remarque : si on modifie la météo, entre le coeur d'un haute pression et le coeur d'un dépression,
	on fait varier l'altitude affichée de +/- 500 ft
	ce qui est très loin d'être négligeable ! (mais est tout à fait réel)
	donc avant de décoller il faut absolument régler le QNH,
	de même en approche d'un aérodrome pour attérissage, le contrôleur ATC indique le QNH (local)
	- https://fr.wikipedia.org/wiki/Phras%C3%A9ologie_de_l%27aviation
*/
	}
	else if (QNH_ok == 0)
	{
		QNH_ok =1;
		TFT480.fillRect(150, 180, 150, 30, HA_SOL); // efface
	}
}


void affi_frq_NAV1()
{
	String s1;
	uint16_t x0=120;
	uint16_t y0=234;
	float f1;

	f1 = vor_frq/1000.0;
	s1= String(f1, 2);
	
	TFT480.fillRect(x0, y0, 55, 18, NOIR); // efface
	TFT480.setFreeFont(FM9);
	TFT480.setTextColor(VERT, NOIR);
	TFT480.drawString(s1, x0, y0);
	TFT480.drawString("MHz", x0+70, y0);
}


void affi_dst_NAV1()
{

	if (vor_dst<0) {vor_dst=0;}
	String s1;
	uint16_t x0=180;
	uint16_t y0=236; 
// rappel: 1 mile marin (NM nautical mile) = 1852m
	float vor__nm = (float)vor_dst / 1852.0;
//Serial.println(vor__nm);

	TFT480.setTextColor(JAUNE, NOIR);
	TFT480.drawString("Nv1", x0-40, y0);
	TFT480.setTextColor(BLANC, NOIR);

	uint8_t nb_decimales =1;
	if (vor__nm >99) {nb_decimales=0;}
	
	s1 = String( vor__nm, nb_decimales);
	TFT480.fillRect(x0, y0, 52, 18, NOIR); // efface
	TFT480.setFreeFont(FM9);
	TFT480.drawString(s1, x0, y0);
	TFT480.drawRoundRect(x0, y0-2, 50, 18, 5, GRIS_FONCE); // encadrement de la valeur affichée
	
	TFT480.drawString("NM", x0+55, y0);
}


void affi_radial_NAV1()
{

	if (vor_actual<0) {vor_actual=0;}
	String s;
	uint16_t x0=260;
	uint16_t y0=236; 

	int16_t alpha1 = vor_actual/100.0;
	
	int16_t alpha2 = alpha1 + 180;
	if (alpha2>360)  {alpha2 -= 360;}

	if (alpha1 == 360) {alpha1 =0;}
	if (alpha2 == 360) {alpha2 =0;}
	
	
//affichage numérique de l'angle
	TFT480.setTextColor(VERT, NOIR);
	//s = String (alpha1 , 0); // 0 décimales
	s = (String) alpha1; 
	TFT480.fillRect(x0, y0, 50, 18, NOIR); // efface
	TFT480.setFreeFont(FM9);
	TFT480.drawString(s, x0+5, y0);
	TFT480.drawRoundRect(x0, y0-2, 40, 18, 5, GRIS_FONCE); // encadrement de la valeur affichée
	TFT480.drawCircle(x0+46, y0, 2, JAUNE); // caractère 'degré'

//affichage numérique de l'angle opposé
	x0+=50;
	TFT480.setTextColor(VERT, NOIR);
	//s = String (alpha2 , 0); // 0 décimales
	s = (String) alpha2;
	TFT480.fillRect(x0, y0, 50, 18, NOIR); // efface
	TFT480.setFreeFont(FM9);
	TFT480.drawString(s, x0+5, y0);
	TFT480.drawRoundRect(x0, y0-2, 40, 18, 5, GRIS_FONCE); // encadrement de la valeur affichée
	TFT480.drawCircle(x0+46, y0, 2, JAUNE);

	
}


void affi_dst_NAV2()
{

	if (ils_dst<0) {ils_dst=0;}
	String s1;
	uint16_t x0=180;
	uint16_t y0=255; 
	float nav_nm;
// rappel: 1 mile marin (NM nautical mile) = 1852m
	//ils_nm = (float)ils_dst / 1852.0;
	//if (ils_nm >99) {ils_nm=0;}
	
	TFT480.setTextColor(JAUNE, NOIR);
	TFT480.drawString("Nv2", x0-40, y0);
	TFT480.drawString("NM", x0+55, y0);
	
	TFT480.setTextColor(BLANC, NOIR);
	s1 = String( ils_nm, 1); // 1  -> 0 décimale
	TFT480.fillRect(x0, y0, 52, 18, NOIR); // efface
	TFT480.setFreeFont(FM9);
	TFT480.drawString(s1, x0, y0);
	TFT480.drawRoundRect(x0, y0-2, 50, 18, 5, GRIS_FONCE); // encadrement de la valeur affichée
		
}


void affi_radial_NAV2()
{

	if (ils_radial<0) {ils_radial=0;}
	String s1;
	uint16_t x0=260;
	uint16_t y0=255; 
		
//affichage numérique
	TFT480.setTextColor(BLEU_CLAIR, NOIR);
	s1 = String (ils_radial/100, 0); // 0 décimales
	TFT480.fillRect(x0, y0, 50, 18, NOIR); // efface
	TFT480.setFreeFont(FM9);
	TFT480.drawString(s1, x0+5, y0);
	TFT480.drawRoundRect(x0, y0-2, 40, 18, 5, GRIS_FONCE); // encadrement de la valeur affichée
	TFT480.drawCircle(x0+46, y0, 2, JAUNE);
}


void affi_alti_NAV2()
{
// hauteur (en ft) conseillée pour l'approche IFR à la distance en nautiques (NM) de la balise Nav2
	String s1;
	uint16_t x=425;
	uint16_t y=5;

	float nav_nm;
	float hauteur; // attention: ne pas confondre hauteur (au dessus du sol) et altitude (au dessus du niveau de la mer)
	nav_nm = (float)ils_dst / 1852.0;
	hauteur = 300.0*nav_nm;
 
	s1 = String( hauteur, 0); // 0  -> 0 décimale
	TFT480.fillRect(x, y, 55, 18, NOIR); // efface

	if (gs_ok != 0) //attention: la valeur différente de 0 n'est pas forcément =1. (variable gérée par FG) donc ne pas écrire "==1"
	{
		TFT480.setTextColor(VERT, NOIR);
		TFT480.setFreeFont(FF1);
		//TFT480.setFreeFont(FM9);
		//TFT480.setFreeFont(FS9);
		//TFT480.setFreeFont(FF5);
		TFT480.drawString(s1, x, y);
	}
}



void affi_indicateurs()
{
	
	TFT480.setFreeFont(FSS9);
	
/*
	TFT480.drawString("CLB | IAS |", 100, 0, 1);

	TFT480.setTextColor(AZUR, NOIR);
	TFT480.drawString("ALT", 210, 0, 1);
	
	TFT480.setTextColor(VERT, NOIR);
	TFT480.drawString(" |", 245, 0, 1);	

	TFT480.setTextColor(AZUR, NOIR);
	TFT480.drawString("HDG", 260, 0, 1);

	TFT480.setTextColor(VERT, NOIR);
	TFT480.drawString(" |", 300, 0, 1);	
*/

	//if (gs_ok != 0)	{ TFT480.setTextColor(VERT, NOIR); } else { TFT480.setTextColor(GRIS_FONCE, NOIR); }
	//TFT480.drawString("ILS", 110, 0);

}


void affi_Airport()
{
	uint16_t n;
	float v1;
	String s1;

	TFT480.fillRect(270, 280, 40, 40, NOIR); // efface
	TFT480.setTextFont(1);
	TFT480.setTextColor(BLEU_CLAIR, NOIR);
	

	s1= liste_bali[num_bali].ID_OACI;
	TFT480.drawString(s1, 270, 280);

	TFT480.fillRect(255, 290, 105, 10, NOIR); // efface
	s1= liste_bali[num_bali].nom;
	TFT480.drawString(s1, 255, 290);

	TFT480.setTextColor(VERT, NOIR);
	
	v1= liste_bali[num_bali].frq_VOR/100.0;
	s1 = String(v1, 2);
	TFT480.drawString(s1, 270, 300);

	v1= liste_bali[num_bali].frq_ILS/100.0;
	s1 = String(v1, 2);
	TFT480.drawString(s1, 270, 310);
	
}



void auto_landing()
{
/**
 voir: https://en.wikipedia.org/wiki/Autoland

 Atterrissage automatique, contrôlé par le bouton1 (si compilé avec la bonne config, voir fonction loop)
 CAPTURE l'avion et le POSE !
 vitesse conseillée : 160kts max
 distance conseillée : entre 20 et 10 nautiques
 hauteur conseillée : 3000ft (= niveau 30)

 à ceci près que l'autopilot se déconnecte automatiquement (par FlightGear) sous 100ft de hauteur
 ( Donc garder le manche en main pour le touché final, en surveillant la vitesse et le pitch
 parce que si le trim de profondeur est mal réglé, à la déconnexion de l'autopilot c'est le plongeon près du sol = crash !
 avec ma manette Thrustmaster et le Citation X, je n'arrive pas à régler le trim !! )

**/
	String s1;
	
	TFT480.setFreeFont(FSS9);
	
	TFT480.setTextColor(GRIS_FONCE, NOIR);
	TFT480.fillRect(80, 0, 265, 16, NOIR); // JAUNE pour test. efface tout le bandeau d'information en haut

	voyant_L.couleur_caract = BLANC;
	voyant_L.affiche(GRIS_TRES_FONCE);

	voyant_G.couleur_caract = BLANC;
	voyant_G.affiche(GRIS_TRES_FONCE);

		
	//float ils_nm =(float)ils_dst / 1852.0;
	
	float delta_LOC = ils_loc / 1000.0; //localizer (en azimut);  remarque: |delta_LOC| jamais > 10, reste bloqué à 10...
	float G1 = ils_glide/1000.0;
// G1 = angle (vertical) sous lequel l'avion est vu depuis la balise ILS
// G1 = +3.00 si angle = 3° = ok
// si G1>3.0 -> avion trop haut, au dessus du plan de descente -> il faut descendre
// si G1<3.0 -> avion trop bas, au dessus du plan de descente -> il ne faut plus descendre
	float delta_GS = 3.0 - G1;


	uint8_t type_balise = liste_bali[num_bali].typ; // pour test si la piste est équipée d'un glide

/**
rappel: codage des Types de balises : (voir fichier FG_data.ino)

_LOC LOCALIZER
_DME DISTANCE MEASURING EQUIPMENT	
_GS  GLIDE SYSTEM
_NDB Non-directional beacon (Balise non directionnelle)

**/	

//-------------------------------------------------------------------------------------------------------	
// gestion des voyants (quelle que soit la position du switch autoland)

	if(au_sol == 0) // c,a,d si l'avion est en vol, pour éviter d'allumer les voyants ILS dès le départ
	{
		if( ((type_balise & _LOC) !=0) && (delta_LOC > -9.5) && (delta_LOC < +9.5) )
		{
			voyant_L.couleur_caract = BLANC;
			voyant_L.affiche(VERT_FONCE);
		}
		else
		{
			voyant_L.couleur_caract = BLANC;
			voyant_L.affiche(GRIS_FONCE);
		}

		
		
		if(((type_balise & _GS) !=0) && (delta_GS > -3.0) && (delta_GS < +3.0) )
		{
			voyant_G.couleur_caract = BLANC;
			voyant_G.affiche(VERT_FONCE);
		}
		else
		{
			voyant_G.couleur_caract = BLANC;
			voyant_G.affiche(GRIS_FONCE);
		}
		
// affichage numérique des valeurs delta_LOC (localizer) et delta_GS (glide)
		TFT480.setTextFont(1);

		uint16_t couleur1=VERT;
		if ((delta_LOC== 10.0) || (delta_LOC== -10.0)) {couleur1=GRIS;} // si bloqué en butée (hors du faisceau donc)
		s1 = "LOC:";
		s1+= String(delta_LOC,2);
		TFT480.setTextColor(couleur1, NOIR);
		TFT480.drawString(s1, 225, 0);

		uint16_t couleur2=ROSE;
		if ((delta_GS<= -5.0) || (delta_GS > 5.0)) {couleur2=GRIS;} // si trop écarté du pla de descente à 3°
		s1 = "GLD:";
		s1+= String(delta_GS,2);
		TFT480.setTextColor(couleur2, NOIR);
		TFT480.drawString(s1, 225, 8);
	}

//-------------------------------------------------------------------------------------------------------
// (si switch1 en position basse, sinon on ne fait rien de plus)

	if ( (autoland != 0) && (((type_balise & _LOC) !=0) || ((type_balise & _GS) !=0)) )
	{
		//TFT480.setTextColor(BLANC); TFT480.setFreeFont(FF1);
		//TFT480.drawString("autoLanding", HA_x0-60, HA_y0+60);

		TFT480.setTextColor(BLANC, NOIR);
		TFT480.drawString("ILS", 110, 0);

		// azimut (localizer)
		if((delta_LOC > -9.5) && (delta_LOC < +9.5) )
		{
			voyant_L.couleur_caract = NOIR;
			voyant_L.affiche(VERT);
		}
		else
		{
			voyant_L.couleur_caract = BLANC;
			voyant_L.affiche(ROUGE);
		}
		
		if((delta_GS > -3.0) && (delta_GS < 3.0) )
		{
			voyant_G.couleur_caract = NOIR;
			voyant_G.affiche(VERT);
		}
		else
		{
			voyant_G.couleur_caract = BLANC;
			voyant_G.affiche(ROUGE);
		}


		if ((gs_ok != 0) && (altitude < 5000) && (ils_nm < 30.0))	
		{
//glide effectif (capture) seulement si l'avion est suffisamment proche de la balise et suffisamment bas

			TFT480.setTextColor(VERT, NOIR);
			TFT480.drawString("ALTI", 110, 0); 
			if(delta_GS < 0) // on force à descendre mais jamais à remonter (on reste alors en palier)
			{
				asel1 = (uint16_t)((altitude/100.0) + 1.3 * delta_GS);
			}
		}	
	

		if ( ((type_balise & _GS) == 0) && (altitude < 5000) && (ils_nm < 30.0) )
		{
// on passe ici si PAS de glide			
// affichage de l'altitude conseillée pour approche sans glide;
// la descente doit alors se faire manuellement par action sur l'autopilot
// toutefois le cap reste automatique

			float alti1 = 3.0*ils_nm + gnd_elv/100.0; // altitude avion doit être = hauteur avion (souhaitée) + altitude terrain (réelle)
			alti1 *= 100;
			String s1 = String(alti1,0);
			TFT480.setTextColor(BLEU_CLAIR, NOIR);
			TFT480.drawString(s1, 300, 0);
		}
		
		if(((type_balise & _LOC) != 0) && (delta_LOC > -9.5) && (delta_LOC < 9.5)) //localizer;  remarque: |delta_LOC| jamais > 10
		{
			// ici l'avion est dans un angle serré (9.5° max) vu de la balise mais pas forcément bien orienté (cap) 
			voyant_L.affiche(VERT);
			TFT480.setFreeFont(FF1);
			
			TFT480.setTextColor(VERT, NOIR); // Autolanding en cours, ok
			TFT480.drawString("LOC", 160, 0);

			hdg1 = (uint16_t)((ils_radial/100.0) + 2.0 * delta_LOC); //corrige la consigne de cap de l'autopilot pour rejoindre puis rester dans l'axe de la piste2

		}
		else
		{
			// indique conditions Autolanding non remplies
			TFT480.setFreeFont(FF1);
			TFT480.setTextColor(GRIS, NOIR); 
			TFT480.drawString("APP", 150, 0);
			//TFT480.drawRect(HA_x0-60, HA_y0+85, 100,20, HA_SOL); // efface le "capture ok"
		} 
	}
//-------------------------------------------------------------------------------------------------------
	

	if (altitude < 1)
	{
		//TFT480.setFreeFont(FF1);
		//TFT480.setTextColor(BLANC, ROUGE);
		//TFT480.drawString("APP", 150, 0); 
		//TFT480.drawRect(HA_x0-60, HA_y0+85, 100,20, HA_SOL); // efface
		//TFT480.drawString("No take off ! ", HA_x0-60, HA_y0+83); // évite de décoller avec l'autoland activé !!
	} 
	
	
}



void affi_localizer()
{
//ILS dans le plan horizontal ; affiche l'erreur de position par rapport à l'axe de la piste

	uint16_t y1 = HA_y0-HA_h-14;

// ils_loc = -10 *1000 .. +10 *1000
// l'affichage doit se faire en HA_x0 +/- HA_w
// 10000/HA_w = 10000/120 = 83

	uint16_t couleur1 = ROSE;
	loc = HA_x0 + ils_loc / 83;
	if ( loc < (HA_x0-HA_w+5)) {loc = HA_x0-HA_w+5; couleur1 = GRIS;}
	if ( loc > (HA_x0+HA_w-5)) {loc= HA_x0+HA_w-5; couleur1 = GRIS;}


	TFT480.fillRect(HA_x0-HA_w, y1, 2*HA_w, 9, GRIS_TRES_FONCE);
	TFT480.drawLine(HA_x0, y1-5, HA_x0, y1+5, BLANC);

	affi_indexV(loc, y1, 1, couleur1); // petit triangle rose en haut, se déplaçant horizontalement

	memo_loc=loc;
}



void affi_glide()  // Nav2
{
/*
 "Glide Path-pente" = ILS dans le plan vertical	
 trace des index horizontaux sur les bords de l'HA
 
 Remarque : dans FG toutes les pistes ne sont pas équipées d'un faisceau ILS GLIDE dans les deux sens d'approche...
 L'affichage de la carte (interne ou externe dans le navigateur) permet de visualiser les faisceaux en question si existants.
 par exemple, dans FG et pour LFMT, seule l'approche par le sud (par la mer) est équipée

 D'autre part, il faut avoir réglé le récepteur NAV2 sur la bonne fréquence.
 Voir le pannel F12 de FD ainsi que les cartes d'aéroport.
 Ce réglage est fait automatiquement par le second instrument "ND" connecté en WiFi, lors de la sélection d'un aérodrome

 On peut aussi lancer FG avec les options suivantes dans FGo! (par exemple):
	--vor_=114.45 		(pour le VOR)
	--ils_=305:108.55 	(pour l'ILS de LFMT piste 31R)
*/

	uint16_t x1 = 75;
	uint16_t x2 = 332;

	gld = HA_y0 - 50*(3.0-ils_glide/1000.0);


	TFT480.fillRect(x1, 30, 9, 2*HA_h, GRIS_TRES_FONCE); // efface
	TFT480.fillRect(x2, 30, 9, 2*HA_h, GRIS_TRES_FONCE); // efface

	TFT480.drawRect(x1, HA_y0, 12, 5, BLANC);
	TFT480.drawRect(x2, HA_y0, 12, 5, BLANC);

	uint16_t couleur1 = ROSE;
	if ( gld < (HA_y0-HA_h+5)) {gld = HA_y0-HA_h+5; couleur1 = GRIS;}
	if ( gld > (HA_y0+HA_h-5)) {gld = HA_y0+HA_h-5; couleur1 = GRIS;}

	affi_indexH(x1, gld, 1, couleur1); 
	affi_indexH(x2+8, gld, -1, couleur1); 		

	memo_gld=gld;
}



void trace_arc_gradu()
{
//arc gradué en haut au centre, indiquant la valeur de l'inclinaison
	
	float angle;
	//Draw_arc_elliptique(HA_x0, 120, 120, 80, 0.6, 2.6, BLANC);


	for(uint8_t n=0; n<9; n++ )
	{
		angle =30+ n*15;  // 1 tiret tous les 15 degrés
		float pourcent = 0.9;
		if (((n+2)%2) == 0) {pourcent = 0.8;}
		
		affi_rayon1(HA_x0, HA_y0+10, 110, degTOrad(angle), pourcent, BLANC, false); // tirets de graduation
	}
}



void rotation1()
{ 
// consigne de cap
// acquisition de l'encodeur pas à pas (1)
	if ( millis() - TEMPO  >= timer1 )
	{
		timer1 = millis();
		bool etat = digitalRead(rot1b);
		if(etat == 0) { hdg1+=1;} else { hdg1-=1;}
		if (hdg1<0){hdg1=355;}
		
		if (hdg1>359){hdg1=0;}		
	}
}



void rotation2()
{
// consigne d'altitude
// acquisition de l'encodeur pas à pas (2)
	if ( millis() - TEMPO  >= timer2 )
	{
		timer2 = millis();
		bool etat = digitalRead(rot2b);
		if(etat == 0) {	asel1+=1; }	else { asel1-=1; }
		if (asel1<1){asel1=1;} // 100 pieds -> 30m
		if (asel1>600){asel1=600;}
	}
}



void init_SDcard()
{
	String s1;
	
	TFT480.fillRect(0, 0, 480, 320, NOIR); // efface
	TFT480.setTextColor(BLANC, NOIR);
	TFT480.setFreeFont(FF1);

	uint16_t y=0;
	
	TFT480.drawString("PRIMARY FLIGHT DISPLAY", 0, y);
	y+=20;

	s1="version " + version;
	TFT480.drawString(s1, 0, y);
	
	y+=40;
	TFT480.setTextColor(VERT, NOIR);
	TFT480.drawString("Init SDcard", 0, y);
	y+=20;
	
	
 	if(!SD.begin())
	{
		TFT480.drawString("Card Mount Failed", 0, y);
		delay (2000);
		TFT480.fillRect(0, 0, 480, 320, NOIR); // efface
		return;
	}
  

    uint8_t cardType = SD.cardType();

    if(cardType == CARD_NONE)
    {
        TFT480.drawString("No SDcard", 0, y);
        delay (2000);
		TFT480.fillRect(0, 0, 480, 320, NOIR); // efface
        return;
    }

    SDcardOk=1;

    TFT480.drawString("SDcard Type: ", 0, y);
	if(cardType == CARD_SD)    {TFT480.drawString("SDSC", 150, y);}
    else if(cardType == CARD_SDHC) {TFT480.drawString("SDHC", 150, y);}

	y+=20;
	
	uint32_t cardSize = SD.cardSize() / (1024 * 1024);
	s1=(String)cardSize + " GB";
	TFT480.drawString("SDcard size: ", 0, y);
	TFT480.drawString(s1, 150, y);

   // listDir(SD, "/", 0);
    
    //Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
    //Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));

	delay (1000);
	TFT480.fillRect(0, 0, 480, 320, NOIR); // efface
	
}


void init_sprites()
{
	// sprites représentant les lettres 'N' 'S' 'E' 'O' qui seront affichées  sur un cercle, inclinées donc.

	SPR_E.setFreeFont(FF1);
	SPR_E.setTextColor(JAUNE);
	SPR_E.createSprite(SPR_W, SPR_H);
	SPR_E.setPivot(SPR_W/2, SPR_H/2);  // Set pivot relative to top left corner of Sprite
	SPR_E.fillSprite(GRIS_TRES_FONCE);
	SPR_E.drawString("E", 2, 1 );

	SPR_N.setFreeFont(FF1);
	SPR_N.setTextColor(JAUNE);
	SPR_N.createSprite(SPR_W, SPR_H);
	SPR_N.setPivot(SPR_W/2, SPR_H/2);
	SPR_N.fillSprite(GRIS_TRES_FONCE);
	SPR_N.drawString("N", 2, 1 );

	SPR_O.setFreeFont(FF1);
	SPR_O.setTextColor(JAUNE);
	SPR_O.createSprite(SPR_W, SPR_H);
	SPR_O.setPivot(SPR_W/2, SPR_H/2);
	SPR_O.fillSprite(GRIS_TRES_FONCE);
	SPR_O.drawString("W", 2, 1 );

	SPR_S.setFreeFont(FF1);
	SPR_S.setTextColor(JAUNE);
	SPR_S.createSprite(SPR_W, SPR_H);
	SPR_S.setPivot(SPR_W/2, SPR_H/2);
	SPR_S.fillSprite(GRIS_TRES_FONCE);
	SPR_S.drawString("S", 2, 1 );
}


void int_to_array(uint16_t valeur_i)
{
// prépare la chaine de caract à zéro terminal pour l'envoi
	String s1= (String) valeur_i;
	var_array[0]=s1[0];
	var_array[1]=s1[1];
	var_array[2]=s1[2];
	var_array[3]=s1[3];
	var_array[4]=0;  // zéro terminal  -> chaine C
}



void setup()
{
    Serial.begin(19200);


	WiFi.persistent(false);
	WiFi.softAP(ssid, password);  // ok, ça marche, crée un réseau. mode privé
	IPAddress IP = WiFi.softAPIP();


	server.on("/switch", HTTP_GET, [](AsyncWebServerRequest *request) // lecture des boutons de l'ESP(3)
	{
// attention: ce code est appelé par une interruption WiFi qui intervient hors timing. Donc pas d'affichage ici !!		
		//size_t n=request->params();  // get arguments count

		argument_recu1 = request->arg("sw1"); // reception de l'argument n°1 de la requête
		switches=argument_recu1;
		v_switches=switches.toInt();
		traitement_switches=1; //positionne ce drapeau afin que le traitement se fasse dans le timming général, pas ici !
		
		int_to_array(0);

//cet array because la fonction "request->send_P()" n'accèpte pas directement le string
//rappel :
//void send_P(int code, const String& contentType, const uint8_t * content, size_t len, AwsTemplateProcessor callback=nullptr);
//void send_P(int code, const String& contentType, PGM_P content, AwsTemplateProcessor callback=nullptr);

		request->send_P(200, "text/plain", var_array); // envoie  comme réponse au client
	});


	server.on("/hdg", HTTP_GET, [](AsyncWebServerRequest *request) // consigne de cap
	{
// attention: ce code est appelé par une interruption WiFi qui intervient hors timing. Donc pas d'affichage ici !!		
		//size_t n=request->params();  // get arguments count

		argument_recu1 = request->arg("a1"); // reception de l'argument n°1 de la requête
		num_bali=argument_recu1.toInt();
		
		int_to_array(hdg1);

//cet array because la fonction "request->send_P()" n'accèpte pas directement le string
//rappel :
//void send_P(int code, const String& contentType, const uint8_t * content, size_t len, AwsTemplateProcessor callback=nullptr);
//void send_P(int code, const String& contentType, PGM_P content, AwsTemplateProcessor callback=nullptr);

		request->send_P(200, "text/plain", var_array); // envoie hdg1 comme réponse au client
	});

// réponses aux requêtes :
// VOIR la fonction "void interroge_WiFi()" dans le code du ND (l'affichage de la carte...)
// pour la réception des données qui suivent
	server.on("/cap", HTTP_GET, [](AsyncWebServerRequest *request)
	{
		int_to_array(cap); // prépare la chaine de caract à zéro terminal pour l'envoi
		request->send_P(200, "text/plain", var_array); // envoie réponse au client
	});

	server.on("/vor", HTTP_GET, [](AsyncWebServerRequest *request)
	{
		int_to_array(vor_actual_deg); // vor_actual_deg
		request->send_P(200, "text/plain", var_array); // envoie réponse au client
	});

	server.on("/ILSactual", HTTP_GET, [](AsyncWebServerRequest *request)
	{
		int_to_array(ils_actual_deg); // ils_actual_deg
		request->send_P(200, "text/plain", var_array); // envoie réponse au client
	});	
 
	server.on("/VORnm", HTTP_GET, [](AsyncWebServerRequest *request) //vor_dst
	{
		uint16_t vor__nm10 = (uint16_t) (vor_dst / 185.2);

		int_to_array(vor__nm10);
		request->send_P(200, "text/plain", var_array); // envoie réponse au client
	});

	server.on("/ILSnm", HTTP_GET, [](AsyncWebServerRequest *request) //ils_dst, pour placer avion sur la carte
	{
		uint16_t ils_nm10 = (uint16_t) (ils_dst / 185.2);

		int_to_array(ils_nm10);
		request->send_P(200, "text/plain", var_array); // envoie réponse au client
	});
	
    server.begin();

/*
// ************* BLUETOOTH *************
	SerialBT.begin("PFD_BT01"); //Bluetooth device name
	//Serial.println("The device started, now you can pair it with bluetooth!");
// *************************************
*/
	pinMode(bouton1, INPUT);

	//pinMode(led1, OUTPUT);

	pinMode(rot1a, INPUT_PULLUP);
	pinMode(rot1b, INPUT_PULLUP);
	pinMode(rot2a, INPUT_PULLUP);
	pinMode(rot2b, INPUT_PULLUP);
	

	attachInterrupt(rot1a, rotation1, RISING);
	attachInterrupt(rot2a, rotation2, RISING);

	TFT480.init();
	TFT480.setRotation(1); // 1 ou 3 -> in[0..3] à voir, suivant disposition de l'afficheur
	TFT480.fillScreen(TFT_BLACK);

	init_SDcard();
	
	init_sprites();

	delay(100);



	TFT480.setTextColor(NOIR, BLANC);

	//TFT480.setFreeFont(FF19);

	init_affi_HA();

	delay(100);


	//TFT480.fillRect(48, 0, 6, 100,   0xFFE0);
	
//	TFT480.fillRect(0, 0, 479, 30, NOIR);
	TFT480.setTextColor(BLANC, NOIR);
	TFT480.setFreeFont(FF19);
	String s1 = "PFD v";
	s1+= version;
//	TFT480.drawString(s1, 70, 3);
	

	Ay_actu=120;
	By_actu=120;

	altitude =0;
	vitesse =0;
	roulis =0;
	tangage =0;
	cap=0;
	vspeed=0; // vitesse verticale

	vor_frq=123500;
	ils_frq=108000;

	vor_dst=1852*102; // 102km
	vor_actual_deg=45;
	vor_actual=45.0 * 100.0;
	ils_dst=20000; // 20km
//	affichages();

	bouton1_etat = digitalRead(bouton1);
	memo_bouton1_etat = bouton1_etat;

	init_FG_bali();
	
	init_affi_autopilot();
	//affi_indicateurs();
	//affi_frq_NAV1();
	
	affi_dst_NAV1();
	affi_radial_NAV1();
	
	affi_dst_NAV2();
	affi_radial_NAV2();

	affi_Airport();

	voyant_L.init(0,0,30,20);
	voyant_L.caract ='L';
	
	
	voyant_G.init(35,0,30,20);
	voyant_G.caract ='G';
	
}




//uint8_t buffer1[200];

uint16_t L=0;
uint16_t C=0;
uint16_t x, y;
uint16_t i1 = 0;

uint8_t p1;

int32_t number = 0;

String parametre;
String s1;
String s2;


void acquisitions()
{
	TFT480.setFreeFont(FM9);
	TFT480.setTextColor(VERT, NOIR);
	
	if(Serial.available() > 14)
	{
		parametre="";
		s1="";
		char octet_in;
		
		while(octet_in != '=')
		{
		  octet_in = Serial.read();
		  if(octet_in != '=')  {parametre += octet_in; }
		}		
		while(octet_in != '\n')
		{
		  octet_in = Serial.read();
		  if(octet_in != '\n') {s1 += octet_in;  }
		}

		if(parametre == "alti" )
		{
			char buf[50];
			s1.toCharArray(buf, 50);
			altitude = atol(buf);
			data_ok |= 1; // positionne bit0
		}
		

		if(parametre == "gnd_elv" )
		{
			char buf[50];
			s1.toCharArray(buf, 50);
			gnd_elv = atol(buf);
			if (gnd_elv <0) {gnd_elv =0;}  
			data_ok |= 2; // positionne bit1
			
			////TFT480.drawString("gnd_elv", 0, 0);
			////s2= (String) gnd_elv;
			////TFT480.fillRect(120, 0, 100, 30, TFT_BLACK);
			////TFT480.drawString(s2, 120, 0);
		}

	
		
		if(parametre == "speed" )
		{
			char buf[50];
			s1.toCharArray(buf, 50);
			vitesse = atol(buf);
			data_ok |= 4; // positionne bit2
			
			////TFT480.drawString("Vitesse", 0, 70);
			////s2= (String) vitesse;
			////TFT480.fillRect(120, 70, 100, 30, TFT_BLACK);
			////TFT480.drawString(s2, 120, 70);
		}


		if(parametre == "pitch" )
		{
			char buf[50];
			s1.toCharArray(buf, 50);
			tangage = atol(buf);
			data_ok |= 8; // positionne bit3
			
			////TFT480.drawString("Tangage", 0, 100);
			////s2= (String) tangage;
			////TFT480.fillRect(120, 100, 100, 30, TFT_BLACK);
			////TFT480.drawString(s2, 120, 100);
		}

		if(parametre == "roll" )
		{
			char buf[50];
			s1.toCharArray(buf, 50);
			roulis = atol(buf);
			data_ok |= 16; // positionne bit4
			
			////TFT480.drawString("Roulis", 0, 130);
			////s2= (String) roulis;
			////TFT480.fillRect(120, 130, 100, 30, TFT_BLACK);
			////TFT480.drawString(s2, 120, 130);
		}



		if(parametre == "heading" ) // /orientation/heading-deg = cap actuel de l'avion ; ne pas confondre avec HDG bug !
		{
			char buf[50];
			s1.toCharArray(buf, 50);
			cap = atol(buf);
			data_ok |= 32; // positionne bit5
			
			////TFT480.drawString("Cap", 0, 160);
			////s2= (String) roulis;
			////TFT480.fillRect(120, 160, 100, 30, TFT_BLACK);
			////TFT480.drawString(s2, 120, 160);
		}
	

		if(parametre == "vspeed" )
		{
			char buf[50];
			s1.toCharArray(buf, 50);
			vspeed = atol(buf);
			data_ok |= 64; // positionne bit6
			
			////TFT480.drawString("vspeed", 0, 170);
			////s2= (String) vspeed;
			////TFT480.fillRect(120, 170, 100, 30, TFT_BLACK);
			////TFT480.drawString(s2, 120, 170);
		}


		if(parametre == "vor_frq" )
		{
			char buf[50];
			s1.toCharArray(buf, 50);
			vor_frq = atol(buf);
			data_ok |= 128; // positionne bit7
			
			////TFT480.drawString("vor_frq", 100, 170, 1);
			////s2= String(vor_frq, 2); //  String(navfrq, 2);
			////TFT480.fillRect(180, 170, 100, 20, TFT_BLACK);
			////TFT480.drawString(s2, 180, 170);
		}
	
		
		if(parametre == "vor_dst" )
		{
			char buf[50];
			s1.toCharArray(buf, 50);
			vor_dst = atol(buf);  // en mètres
			
			// Serial.println(vor_dst);
			
			data_ok |= 256; // positionne bit8
			
			////TFT480.drawString("vor_dst", 0, 170, 1);
			////s2= (String) vor_dst;
			////TFT480.fillRect(120, 170, 100, 30, TFT_BLACK);
			////TFT480.drawString(s2, 120, 170);
		}


		x=100;
		
		if(parametre == "ils_dst" )
		{
			char buf[50];
			s1.toCharArray(buf, 50);
			ils_dst = atol(buf);
			ils_nm = (float)ils_dst / 1852.0;
			data_ok |= 512; // positionne bit9

			////y=160;
			////TFT480.drawString("dst:", x, y);
			////s2= (String) ils_dst;
			
			////TFT480.fillRect(x+70, y, 100, 20, TFT_BLACK);
			////TFT480.drawString(s2, x+70, y);

		}


		if(parametre == "ils_loc" )
		{
// erreur d'angle (/ à l'axe de la piste) dans le plan horizontal sous lequel l'antenne ILS au sol voit l'avion; Doit être 0°
// en fait varie dans la plage [-10.0 .. +10.0] avec un facteur 1000 écrit dans "hardvare4.xml"
			char buf[50];
			s1.toCharArray(buf, 50);
			
			ils_loc = atol(buf); // = 1000* [-10.0 .. +10.0] c.a.d = [-10000 .. +10000];
			//Attention : |ils_loc| jamais > 10, même si l'angle réel est bien plus grand, de l'ordre de 90° !!
			
			data_ok |= 1024; // positionne bit10

			////y=160;
			////TFT480.drawString("angl_H", x, y);
			////s2= String (ils_loc/1000, 2);
			
			////TFT480.fillRect(x+70, y, 100, 20, TFT_BLACK);
			////TFT480.drawString(s2, x+70, y);
		}


		if(parametre == "ils_glide" )
		{
//angle dans le plan vertical sous lequel l'antenne au sol (ILS GLIDE) voit l'avion; Doit être de 3° en finale
//c'est l'inclinaison du plan de descente effectif
			char buf[50];
			s1.toCharArray(buf, 50);
			ils_glide = atol(buf);
			data_ok |= 2048; // positionne bit11

			////y=190;
			////TFT480.drawString("angl_V", x, y);
			////s2= String (ils_glide/1000, 2);
			
			////TFT480.fillRect(x+70, y, 100, 20, TFT_BLACK);
			////TFT480.drawString(s2, x+70, y);
		}


		if(parametre == "gs_ok" )
		{
// indique si le signal ILS  est reçu
// un test en vol permet de voir que le signal décroche au delà de 27.2 nautiques soit 50km de distance 
			char buf[50];
			s1.toCharArray(buf, 50);
			gs_ok = atol(buf);
			data_ok |= 4096; // positionne bit12

			////y=190;
			////TFT480.drawString("angl_V", x, y);
			////s2= String (ils_glide/1000, 2);
			
			////TFT480.fillRect(x+70, y, 100, 20, TFT_BLACK);
			////TFT480.drawString(s2, x+70, y);
		}		
	

		if(parametre == "vor_actual" )
		{
//angle dans le plan horizontal sous lequel la station (VOR) voit l'avion
			char buf[50];
			s1.toCharArray(buf, 50);
			vor_actual = atol(buf);
			vor_actual_deg = vor_actual/100;
			data_ok |= 8192; // positionne bit13

			////y=190;
			////TFT480.drawString("radial", x, y);
			////s2= String (ils_radial/100, 2);
			
			////TFT480.fillRect(x+70, y, 100, 20, TFT_BLACK);
			////TFT480.drawString(s2, x+70, y);
		}


		if(parametre == "ils_radial" )
		{
//angle dans le plan horizontal sous lequel l'antenne au sol (LOC) voit l'avion
			char buf[50];
			s1.toCharArray(buf, 50);
			ils_radial = atol(buf);
			data_ok |= 16384; // positionne bit14

			////y=190;
			////TFT480.drawString("radial", x, y);
			////s2= String (ils_radial/100, 2);
			
			////TFT480.fillRect(x+70, y, 100, 20, TFT_BLACK);
			////TFT480.drawString(s2, x+70, y);
		}


		if(parametre == "ils_actual" )
		{
//angle dans le plan horizontal sous lequel l'antenne au sol (LOC) voit l'avion
			char buf[50];
			s1.toCharArray(buf, 50);
			ils_actual = atol(buf);
			ils_actual_deg = ils_actual/100;
			data_ok |= 32768; // positionne bit15

			////y=190;
			////TFT480.drawString("actual", x, y);
			////s2= String (ils_actual/100, 2); // ok
			
			////TFT480.fillRect(x+70, y, 100, 20, TFT_BLACK);
			////TFT480.drawString(s2, x+70, y);
		}

	}

	

	delay(3); // important sinon ne recevra pas la totalité des données (qui se fait en plusieurs passes)
	
// pour test	
////TFT480.drawString("data= ", 90, 40);
////s2= (String) data_ok;
////TFT480.fillRect(140, 40, 50, 30, TFT_BLACK);
////TFT480.drawString(s2, 150, 40);
	
}



void data_out()
{
// destination FlightGear par la liaison série USB

	Serial.print(hdg1);  // consigne de Cap -> autopilot
	Serial.print(',');
	 
	Serial.print(asel1); // consigne d'altitude -> autopilot
	Serial.print(',');

	float v1 = liste_bali[num_bali].frq_VOR/100.0;
	Serial.print(v1);  // écrit la fréquence VOR dans Nav1
	Serial.print(',');

	float v2 = liste_bali[num_bali].frq_ILS/100.0;
	Serial.print(v2); // écrit la fréquence ILS dans Nav2
	Serial.print('\n');	
}



void affichages()
{
	if (roulis < -45)	{roulis = -45;}
	if (roulis > 45)	{roulis = 45;}

	if (tangage < -30)	{tangage = -30;}
	if (tangage > 30)	{tangage = 30;}
	
	affi_HA();
	affi_vitesse();
	affi_altitude();
	trace_arc_gradu();
	affi_cap();
	affi_vt_verticale();
	affi_acceleration();
	affi_autopilot();
	affi_asel();
	dessine_avion();
	affi_indicateurs();
	affi_radial_NAV1();
	affi_radial_NAV2();
	affi_Airport();

	affi_switches();
}


void affi_nop()
{
	for(int8_t dy=-2; dy<3; dy++)
	{
		TFT480.drawLine(HA_x0-HA_w, HA_y0-HA_h +dy, HA_x0 +HA_w, HA_y0 +HA_h +dy, ROUGE);
		TFT480.drawLine(HA_x0-HA_w, HA_y0+HA_h +dy, HA_x0 +HA_w, HA_y0 -HA_h +dy, ROUGE);
	}

	//TFT480.fillRect(0, 0, 239, 30, NOIR);
	TFT480.setTextColor(BLANC, ROUGE);
	TFT480.setFreeFont(FF18);
	TFT480.drawString("No Data", HA_x0-34, HA_y0+30);
}




void init_affi_autopilot()
{

	TFT480.setFreeFont(FF1);
	TFT480.setTextColor(JAUNE, GRIS_AF);

// ALT
	
	//TFT480.drawString("ALT", x_autopilot, 260, 1);
	//TFT480.drawRoundRect(x_autopilot-4, 255, 45, 42, 5, BLANC);
}



void affi_autopilot()
{
// dans le petit cercle en bas à gauche	:
// affiche HDG (flèche jaune), piste (rectangle bleu), VOR (Nav1, ligne verte)
	
	uint16_t x0=70; // 70
	uint16_t y0=248; // 255
	
	TFT480.setFreeFont(FF1);

	TFT480.fillRect(x0, y0+2, 70, 80, NOIR); // efface
	
	TFT480.setTextColor(JAUNE, NOIR);
	TFT480.drawString("HDG", x0, y0-18);
	//TFT480.drawRoundRect(x0-4, 255, 45, 42, 5, BLANC);

	TFT480.drawCircle(x0+35, y0+34, 30, BLANC);
	
	String s1 =(String)hdg1;
	TFT480.setTextColor(BLANC, NOIR);
	
	TFT480.drawString(s1, x0+18, y0+35);

	TFT480.setTextColor(BLANC, NOIR);
	TFT480.drawString("N", x0+30, y0-5);

	TFT480.setTextColor(BLANC, NOIR);
	TFT480.drawString("S", x0+30, y0+60);

	TFT480.setTextColor(BLANC, NOIR);
	TFT480.drawString("E", x0+60, y0+27);
	TFT480.drawString("W", x0, y0+27);

	uint16_t x1,y1;
	
	
// flèche jaune = règlage HDG de l'autopilot
	float angle1 = 90-hdg1;
	affi_rayon2(x0+35, y0+34, 0, 30, angle1, JAUNE, 0); // tige de la flèche
	affi_pointe(x0+35, y0+34, 30, angle1, JAUNE); // pointe triangulaire en bout de flèche

// rectangle bleu, très fin -> orientation ('radial') de l'axe de la piste
	float angle2 = 90+ils_radial/100.0;
	affi_rectangle_incline(x0+35, y0+34, 35, angle2, BLEU_CLAIR);

// trait vert -> radial du VOR (dont la fréquence radio est réglée dans Nav1)
	float angle3 = 90+vor_actual/100.0;
	//affi_rectangle_incline(x0+35, y0+34, 35, angle3, ROUGE);
	affi_rayon2(x0+35, y0+34, -28, 28, -angle3, VERT, 0); 

	
	
// ALTITUDE

// ce paramétrage permet de régler la valeur de 0 à 200 (soit 0 à 20000 pieds soit 6000m environ)
//pour aller plus haut on peut toujours utiliser l'interface de FG (touche F11...)

/*
	String s2 =(String)asel1;
	TFT480.setTextColor(ROSE, NOIR);
	TFT480.fillRect(x_autopilot, 280, 40, 15, NOIR); // efface
	TFT480.drawString(s2, x_autopilot, 278);
*/
	affi_asel();

	data_out();  // ** à commenter le cas échéant pour libérer le port série lors de la mise au point **
	
}



void toutes_les_10s()
{
/*
	uint8_t aa1;
	aa1=70;
	SerialBT.write(aa1);
*/	
	//TFT480.fillCircle(450, 310, 5, VERT);
	//delay (100);
	//TFT480.fillCircle(450, 310, 5, NOIR);

	// write_TFT_on_SDcard(); // commenté le temps de trouver le moyen de lancer par bouton poussoir (manque pin libre...)

	
}



void toutes_les_1s()
{
	nb_secondes++;

//// size_t write(uint8_t c);
//// size_t write(const uint8_t *buffer, size_t size);

	if(nb_secondes>10)
	{
		nb_secondes=0;
		toutes_les_10s();
	}

	dV =10*(vitesse - memo_vitesse);
	memo_vitesse = vitesse;

	auto_landing(); // ne sera effectif que si la variable autoland !=0; toutefois la fonction est toujours appelée afin d'actualiser les affichages
 }

/** ================================================================== 
 variables à gérer obligatoirement */

uint8_t TEST_AFFI=1;// =0 pour un fonctionnement normal; =1 pour tester les affichages et la fluidité

//le nombre de ports GPIO libres étant atteint, on va utiiser un switch unique pour deux fonctions :
uint8_t fonction_bt1 = 1; // 0=saisie écran ; 1=autoland (atterrissage automatique)

/** ==================================================================  */

uint16_t t=0; // temps -> rebouclera si dépassement    
void loop()
{
	//if (SerialBT.available()) {	Serial.write(SerialBT.read()); }

//le bouton connecté à CET ESP32 -ci est partagé entre deux fonctions, voir ci-dessus "variables à gérer obligatoirement"
	bouton1_etat = digitalRead(bouton1);

	if (bouton1_etat != memo_bouton1_etat)
	{
		memo_bouton1_etat = bouton1_etat;
		if (bouton1_etat==0)
		{
			TFT480.fillCircle(455, 310, 5, VERT);
			

			if (fonction_bt1 == 0) {write_TFT_on_SDcard(); }
			if (fonction_bt1 == 1)
			{
				memo_hdg1 = hdg1; // mémorise le cap avant de passer la main à l'autoland
				autoland=1;
			}
		}
		if (bouton1_etat==1)
		{
			TFT480.fillCircle(455, 310, 5, NOIR);
			if (fonction_bt1=1)
			{
// si, suite à un enclenchement non correct de l'autoland (sur un mauvais sens du localizer en particulier)	le cap a été boulversé
// on le remet à sa valeur mémorisée. Evite de devoir tourner le bouton pendant une heure avec l'avion qui part à l'aventure !			
				hdg1 = memo_hdg1; 
				autoland=0;
				init_affi_HA();
				asel1=30;
				voyant_L.affiche(GRIS_FONCE);
				voyant_G.affiche(GRIS_FONCE);
			}
		}
	}

	if (traitement_switches ==1)
// le positionnement de ce drapeau se fait dans l'interruption WiFi "server.on("/switch"..."
// définie dans le setup()
// la commande, en amont, provient de l'ESP n°3 qui gère les switches
	{
		traitement_switches =0;
		if(v_switches != memo_v_switches)
		{
			memo_v_switches = v_switches;
			if ((v_switches & 1)==1)
			{
				if (hdg1<330) {hdg1 +=30;} else {hdg1-=330 ;} // la barre à babord 30° !
			}
			if ((v_switches & 2)==2)
			{
				if (hdg1>30) {hdg1 -=30;} else {hdg1+=330;} // la barre à tribord 30° !
			}

			if ((v_switches & 4)==4)
			{
				if (asel1<600) {asel1 +=10;} // consigne d'altitude +1000ft
			}
			if ((v_switches & 8)==8)
			{
				if (asel1>10) {asel1 -=10;}  // consigne d'altitude -1000ft
			}
			
		}
	}
	
	
	temps_ecoule = micros() - memo_micros;
	if (temps_ecoule  >= 1E6)  // (>=) et pas strictement égal (==) parce qu'alors on rate l'instant en question
	{
		memo_micros = micros();
		toutes_les_1s();
	}

	if (dV > acceleration) {acceleration++;}
	if (dV < acceleration) {acceleration--;}

 
//---------------------------------------------------------------------------------------
	if(TEST_AFFI==0) // pour test des affichages:
	{  

	// les valeurs de ces paramètres peuvent être modifiées afin de tester tel ou tel point particulier
			
		vitesse = 100 - 100*cos(t/200.0);
		//vitesse = 100;
		
		altitude = 1000 - 800*cos(t/400.0);
		vspeed = 80*sin(t/100.0);
		
		tangage =20.0*sin(t/87.0);
		roulis = 35.0*sin(t/35.0);

		//cap =180.0+180.0*sin(t/300.0);
		//vor_dst=27780+27780*sin(t/500.0) ;// test 400 000 nautiques max = 15km
		
		vor_actual_deg= 45;// 100*(180 + 180.0*sin(t/250.0));
		
		ils_dst=27780+27780*sin(t/500.0) ;// test 400 000 nautiques max = 15km
		ils_radial=304.0 *100.0;

		ils_actual_deg+=1.0;
		if (ils_actual_deg >359) {ils_actual_deg=0;}
		cap = ils_actual_deg -90;
		if (cap <0) {cap+=360;}
		
		data_ok=65535;
		ils_glide= (int16_t) (3000 + 2000*sin(t/77.0) ); // soit 5°... 4°
		ils_loc = 10E+3*sin(t/63.0); // soit -10°... +10°
		gs_ok=1;

		affichages();

		t++;

		TFT480.setTextColor(JAUNE, BLEU);
		TFT480.setFreeFont(FF1);
		TFT480.drawString("mode TEST", 0, 0);


		if (dV > acceleration) {acceleration++;}
		if (dV < acceleration) {acceleration--;}
		delay(20);
		
	}
	
//---------------------------------------------------------------------------------------
	else  // FONCTIONNEMENT NORMAL
	{
		compteur1+=1;
		if (compteur1 >= 100)  // tous les 1000 passages dans la boucle 
		{
			compteur1=0;
			affi_autopilot();
		}		

		acquisitions();

		
/** ----------------------------------------------
 pour tester si data_ok à bien la valeur attendue, c.a.d si toutes les acquisitions sont effectuées
 ce qui est le cas lorsque le dialogue avec le serveur de protocole FG est correctement programmé
 sachant que ce ne sont pas les pièges qui manquent !
 Les 6 lignes de code qui suivent peuvent faire gagner une journée...
 Il suffit d'analyser la valeur affichée en binaire pour voir les bits qui restent à 0 -> problèmes (à priori de libellés)
 Il doit y avoir une correspondance stricte entre les noms de variables ici et celles émises par le fichier hardware4.xml
*/ 
////TFT480.setFreeFont(FM9);
////TFT480.setTextColor(VERT, NOIR);
////TFT480.drawString("d_sum=", 0, 0);
////s2= (String) data_ok;
////TFT480.fillRect(70, 0, 50, 30, TFT_BLACK);
////TFT480.drawString(s2, 70, 0);
/** ---------------------------------------------- **/
				
		if (data_ok == 65535)
// 32767 = 1+2+4+8+16+32+64+128+256+512+1024+2048+4096+8192+16384+32768 = 0b 11111111 11111111 = (2^16)-1 voir fonction "acquisitions()"
		{
			TFT480.fillCircle(440, 310, 5, VERT);
			if (attente_data == 1)
			{
				attente_data=0;
				TFT480.fillScreen(TFT_BLACK);
				init_affi_HA();
				init_affi_autopilot();
			}

			affichages();
			data_ok=0;
		}
		else
		{
			TFT480.fillCircle(440, 310, 5, ROUGE);
			if(attente_data==1)
			{
				affi_nop();
				RAZ_variables();
				affichages();
				delay(100);
			}
		}

		if ((vitesse > 100) && (altitude > 500)) {au_sol =0;} 
	}

//---------------------------------------------------------------------------------------

}


/** ***************************************************************************************
	CLASS VOYANT  // affiche un nombre ou un petit texte dans un rectangle
	ainsi que (en plus petit) deux valeurs supplémentaires, par ex: les valeurs mini et maxi					
********************************************************************************************/

// Constructeur
VOYANT::VOYANT()
{

}



void VOYANT::init(uint16_t xi, uint16_t yi, uint16_t dxi, uint16_t dyi)
{
	x0 = xi;
	y0 = yi;
	dx = dxi;
	dy = dyi;

	TFT480.setTextColor(BLANC, NOIR);
}



void VOYANT::affiche(uint16_t couleur_i)
{
	TFT480.fillRoundRect(x0, y0, dx, dy, 3, couleur_i);
	if (caract !=' ')
	{
		TFT480.setTextColor(BLANC);
		TFT480.setFreeFont(FM9);
		TFT480.drawChar(x0+8, y0+14, caract, couleur_caract, couleur_i, 1);
	}
}




/**
-----------------------------------------------------------------------------------------------------------

  ci-dessous contenu du fichier (pour Linux): "/usr/share/games/protocol/hardware4.xml"  qui va bien pour CE programme

  IMPORTANT : Lorsque je fais évoluer le programme principal (ci-dessus) je modifie également (le cas échéant) le fichier hardware4.xml (ci-dessous)
  je ne numérote pas le fichier ci-dessous. Donc pensez à toujours utiliser celui qui est présent ici
  (en le recopiant au bon endroit, ici ce n'est qu'un commentaire non fonctionnel en tant que tel).
  en effet les variables transmises par FG doivent correspondre exactement à ce qu'attend l'ESP32, voir la fonction "void acquisitions()"


  FG doit être lancé avec les paramètres suivants :

	--generic=serial,in,2,/dev/ttyUSB0,9600,hardware4
	--generic=serial,out,2,/dev/ttyUSB0,9600,hardware4

IMPORTANT :
	Il faut ajouter ces DEUX lignes dans un lanceur (tel que "FGo!") pour obtenir un fonctionnement bidirectionnel

Remarques :
  - le nom du fichier "hardware4.xml" peut être autre chose pourvu que le paramètre de lancement corresponde exactement au nom choisi
  - le fichier en question n'existe pas par défaut dans le répertoire "/usr/share/games/protocol/", il faut le créer.


Astuce : pour gagner du temps au décollage, on peut lancer FG avec les options en ligne de commande suivantes (en plus de celles vues plus haut):

	--prop:/autopilot/locks/heading=HDG
	--prop:/autopilot/locks/altitude=ALT
	--prop:/autopilot/locks/speed-ctrl=true
	--prop:/autopilot/settings/target-speed-kt=160

ce qui configure l'autopitot correctement en adéquation avec notre PFD
les deux premières lignes en particulier permette de piloter l'avion depuis le PFD en tournant les encoreurs rotatifs du cap et de l'altitude
elles permetttent également de contrôler l'avion en mode autopilot et landing auto ILS (guidage glide et localizer)	  

-----------------------------------------------------------------------------------------------------------

<?xml version="1.0"?>

<PropertyList>

<generic>
    <output>
        <binary_mode>false</binary_mode>
        <line_separator>\n</line_separator>
        <var_separator>\n</var_separator>
        <preamble></preamble>
        <postamble></postamble>

        <chunk>
            <name>Altitude</name>
            <node>/instrumentation/altimeter/indicated-altitude-ft</node>
            <type>integer</type>
            <format>alti=%i</format>
        </chunk>

		<chunk>
			<name>ground_elevation</name>
			<type>integer</type>
			<node>/position/ground-elev-ft</node>
			<format>gnd_elv=%i</format>
		</chunk>
        

        <chunk>
            <name>Speed</name>
            <node>/velocities/airspeed-kt</node>
            <type>integer</type>
            <format>speed=%i</format>
        </chunk>

        <chunk>
            <name>Tangage</name>
            <node>/orientation/pitch-deg</node>
            <type>integer</type>
            <format>pitch=%i</format>
        </chunk>

        <chunk>
            <name>Roulis</name>
            <node>/orientation/roll-deg</node>
            <type>integer</type>
            <format>roll=%i</format>
        </chunk>

        <chunk>
            <name>Cap</name>
            <node>/orientation/heading-deg</node>
            <type>integer</type>
            <format>heading=%i</format>
        </chunk>

        <chunk>
            <name>Vertical_speed</name>
            <node>/velocities/vertical-speed-fps</node>
            <type>integer</type>
            <format>vspeed=%i</format>
        </chunk>

 		<chunk>
			<name>vor_frq</name>
			<type>integer</type>
			<node>/instrumentation/nav/frequencies/selected-mhz</node>
			<factor>1000</factor>
			<format>vor_frq=%i</format>
		</chunk>       


        <chunk>
            <name>vor_dst</name>
            <node>/instrumentation/nav/nav-distance</node>
            <type>integer</type>
            <format>vor_dst=%i</format>
        </chunk>


		<chunk>
			<name>ils_dst</name>
			<type>integer</type>
			<node>/instrumentation/nav[1]/gs-distance</node>
			<format>ils_dst=%i</format>
		</chunk>
 

		<chunk>
			<name>ils_loc</name>
			<type>integer</type>
			<node>/instrumentation/nav[1]/heading-needle-deflection</node>
			<factor>1000</factor>
			<format>ils_loc=%i</format>
		</chunk>

		
		<chunk>
			<name>ils_glide</name>
			<type>integer</type>
			<node>/instrumentation/nav[1]/gs-direct-deg</node>
			<factor>1000</factor>
			<format>ils_glide=%i</format>
		</chunk>


		<chunk>
			<name>GS_OK</name>
			<type>integer</type>
			<node>/instrumentation/nav[1]/gs-in-range</node>
			<format>gs_ok=%i</format>
		</chunk>


		<chunk>
			<name>Nav1_actual</name>
			<type>integer</type>
			<node>/instrumentation/nav/radials/actual-deg</node>
			<factor>100</factor>
			<format>vor_actual=%i</format>
		</chunk>


		<chunk>
			<name>ils_radial</name>
			<type>integer</type>
			<node>/instrumentation/nav[1]/radials/target-radial-deg</node>
			<factor>100</factor>
			<format>ils_radial=%i</format>
		</chunk>

		<chunk>
			<name>ils_actual</name>
			<type>integer</type>
			<node>/instrumentation/nav[1]/radials/actual-deg</node>
			<factor>100</factor>
			<format>ils_actual=%i</format>
		</chunk>

		

    </output>


    <input>
		<line_separator>\n</line_separator>
        <var_separator>,</var_separator>

       <chunk>
         <name>heading_bug</name>
         <node>/autopilot/settings/heading-bug-deg</node>
         <type>integer</type>
         <relative>false</relative>
     </chunk>

     <chunk>
         <name>asel</name>
         <node>/autopilot/settings/asel</node>
         <type>integer</type>
         <relative>false</relative>
     </chunk>

     
     <chunk>
         <name>Nav1_frq</name>
         <node>/instrumentation/nav/frequencies/selected-mhz</node>
         <type>float</type>
         <relative>false</relative>
     </chunk>


     <chunk>
         <name>Nav2_frq</name>
         <node>/instrumentation/nav[1]/frequencies/selected-mhz</node>
         <type>float</type>
         <relative>false</relative>
     </chunk>
     

    

   </input>



</generic>

</PropertyList>


**/
