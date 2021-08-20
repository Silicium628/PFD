
/*---------------------------------------------------------------------------------------
	ND.ino  - Navigation Display pour ESP32 - pour Afficheur TFT480 480x320
	
	par Silicium628
	Ce logiciel est libre et gratuit sous licence GNU GPL
	
---------------------------------------------------------------------------------------	
	Pour les #includes issus de l'univers Arduino (que je ne fournis pas), il faut voir au cas par cas.
	(drivers d'affichage en particulier)

---------------------------------------------------------------------------------------
	De petites images à placer sur la SDcard centrées sur les aérodromes proviennent de OpenStreetMap
	OpenStreetMap® est un ensemble de données ouvertes,
	disponibles sous la licence libre Open Data Commons Open Database License (ODbL)
	accordée par la Fondation OpenStreetMap (OSMF).

	Voir ce lien pour plus de détails :
	https://www.openstreetmap.org/copyright
---------------------------------------------------------------------------------------
*/


/**
==========================================================================================================
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
==========================================================================================================
**/

/*
Notes :

- ce Navigation Display reçoit toutes les données par WiFi, émises par l'ESP du PFD. Il n'est donc pas nécessaire de le connecter
sur un port USB, il peut être simplement alimenté par un adaptateur 5V, ou sur l'alim 5V du PFD (en amont du régulateur 3V3 )
si toutefois le port USB0 (et son câble) qui alimentent le PFD peuvent fournir suffisamment de courant.
*/

	String version="16.0";
// le numéro de version doit être identique à celui du PFD (Primary Flight Display)

// affichage carte + zoom
// selection des carte par encodeur rotatif (ce qui met à jour les fréquences radio et les paramètres de la piste, et ILS) 
// affiche avion sur la carte
// affiche faisceau ILS sur la carte
// affiche cercles 10 , 20, 40, 80, 160, 320NM sur la carte
// v14.0 les fichiers principaux ont été renommés par soucis de clarté
// v15.6 ajout de cartes .bmp (à copier sur la SDcard)

// todo : mieux centrer certaines cartes (j'ai ajouté une mire dans le .zip)


#include <stdint.h>
#include <TFT_eSPI.h> // Hardware-specific library

#include "Free_Fonts.h"

#include "FS.h"
#include "SD.h"
#include "SPI.h"

#include "ND.h"
#include "FG_data.h"

#include <WiFi.h>
#include <HTTPClient.h> // nous nous connecterons en tant que client au serveur PFD.
// Remarque: les fonctions WiFi consomment 50% de la mémoire flash (de programme)

const char* ssid = "PFD_srv"; 
const char* password = "72r4TsJ28";


// addresse IP + chemins URL du PFD:
const char* srvName_hdg = "http://192.168.4.1/hdg";
const char* srvName_cap = "http://192.168.4.1/cap";
const char* srvName_rd1 = "http://192.168.4.1/vor";
const char* srvName_vor_nm = "http://192.168.4.1/VORnm";
const char* srvName_ils_nm = "http://192.168.4.1/ILSnm";
const char* srvName_ils_actual = "http://192.168.4.1/ILSactual";
const char* srvName_sw = "http://192.168.4.1/switch";



String recp_hdg; // heading bug - HDG = consigne de cap
String recp_cap; //	cap actuel (orientation/Nord du nez de l'avion) - sens horaire
String recp_vor; // radial 1
String recp_vor1nm;
String recp_ILSnm;
String recp_ILS_actual;

TFT_eSprite SPR_VOR = TFT_eSprite(&TFT480);
TFT_eSprite SPR_avion = TFT_eSprite(&TFT480); // le petit avion sur la carte

uint16_t memo_img[20*20];

TFT_eSprite SPR_E = TFT_eSprite(&SPR_VOR);  // Declare Sprite object "SPR_11" with pointer to "TFT480" object
TFT_eSprite SPR_N = TFT_eSprite(&SPR_VOR);
TFT_eSprite SPR_O = TFT_eSprite(&SPR_VOR);
TFT_eSprite SPR_S = TFT_eSprite(&SPR_VOR);

TFT_eSprite SPR_3 = TFT_eSprite(&SPR_VOR);  
TFT_eSprite SPR_6 = TFT_eSprite(&SPR_VOR);
TFT_eSprite SPR_12 = TFT_eSprite(&SPR_VOR);
TFT_eSprite SPR_15 = TFT_eSprite(&SPR_VOR);
TFT_eSprite SPR_21 = TFT_eSprite(&SPR_VOR);  
TFT_eSprite SPR_24 = TFT_eSprite(&SPR_VOR);
TFT_eSprite SPR_30 = TFT_eSprite(&SPR_VOR);
TFT_eSprite SPR_33 = TFT_eSprite(&SPR_VOR);

Cadre cadre_top1;
Cadre cadre_top2;

Cadre cadre_00;
Cadre cadre_01;

Cadre cadre_10;
Cadre cadre_11;

Cadre cadre_20;
Cadre cadre_21;

Cadre cadre_cap1;
Cadre cadre_hdg1;
Cadre cadre_rd1;

Cadre cadre_bas1;
Cadre cadre_bas2;


// COULEURS RGB565
// Outil de sélection -> http://www.barth-dev.de/online/rgb565-color-picker/

#define NOIR   		0x0000
#define MARRON   	0x9240
#define ROUGE  		0xF800
#define ROSE		0xFBDD
#define ORANGE 		0xFBC0
#define JAUNE   	0xFFE0
#define JAUNE_PALE  0xF7F4
#define VERT   		0x07E0
#define PAMPA		0xBED6
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


// Width and height of sprite
#define SPR_W 25
#define SPR_H 16

int16_t num_bali=0; // peut être négatif lors de l'int de rotation de l'encodeur


uint32_t memo_micros = 0;
uint32_t temps_ecoule;
uint16_t nb_100ms=0;
uint16_t nb_secondes=0;

float cap;
float memo_cap;

int32_t vor_dst;  // int et pas uint -> peut être négatif volontairement
uint32_t vor_frq;
uint32_t vor_radial; // 0..360
uint32_t memo_vor_radial;

int32_t ils_dst; // int et pas uint -> peut être négatif volontairement
float ils_nm;
uint32_t ils_frq;
uint32_t ils_actual; // direction de l'avion vu de la piste
float ils_orientation; // orientation de la piste


int16_t hdg1  = 150;  // consigne cap
int16_t memo1_hdg1;	// pour le changement manuel
int16_t memo2_hdg1; // pour l'autoland

uint8_t SDcardOk=0;
uint8_t gs_ok;

uint8_t refresh_ND =1;
uint8_t refresh_Encoche =1;
uint8_t refresh_carte =1;


uint16_t x_avion; // le petit avion qui se déplace sur la carte
uint16_t y_avion;
uint16_t memo_x_avion;
uint16_t memo_y_avion;

int8_t zoom; // pour la carte; = numéro de la carte .bmp
float px_par_NM; 

const int bouton1 = 36;  // attention: le GPIO 36 n'a pas de R de pullup interne, il faut en câbler une au +3V3
bool bouton1_etat;
bool memo_bouton1_etat;

// deux encodeurs rotatifs pas à pas
const int rot1a = 32;  // GPIO32 -> câbler une R au +3V3
const int rot1b = 33;  // GPIO33 -> câbler une R au +3V3

const int rot2a = 35;  // GPIO35 -> câbler une R au +3V3
const int rot2b = 34;  // GPIO34 -> câbler une R au +3V3

//const int led1 = 25; // GPIO15

#define TEMPO 20 // tempo anti-rebond pour l'acquisition des encodeurs rotatifs
volatile uint32_t timer1 = 0;
volatile uint32_t timer2 = 0;

uint16_t compteur1;
uint8_t premier_passage=1;

uint32_t bmp_offset = 0;

String switches="null"; // boutons connectés au 3eme ESP32, reçus par WiFi


void RAZ_variables()
{
	vor_dst=0;
	vor_radial=0;
	memo_vor_radial=0;
	ils_dst=0;
	hdg1=0;  // consigne cap
}


void httpGetHDG()
{
	HTTPClient http;
	char c1[4];

	String s1= "?a1=";  // a1 (nom arbitraire..) pour désigner l'argument n°1 envoyé dans l'URL de la requête
	s1+= num_bali;
	
	//n_balise_array[0]=s1[0];
	//n_balise_array[1]=s1[1];
	//n_balise_array[2]=s1[2];
	//n_balise_array[3]=0;  // zéro terminal  -> string

	http.begin(srvName_hdg+s1);
	int httpResponseCode = http.GET();
	if (httpResponseCode>0)	{ recp_hdg = http.getString();}
	http.end();
}



void httpGetCap()
{
	HTTPClient http;
	http.begin(srvName_cap);
	int httpResponseCode = http.GET();
	if (httpResponseCode>0)	{ recp_cap = http.getString();	}
	http.end();
}


void httpGetRd1() // radial 1
{
	HTTPClient http;
	http.begin(srvName_rd1);
	int httpResponseCode = http.GET();
	if (httpResponseCode>0)	{ recp_vor = http.getString();	}
	http.end();
}


void httpGetVor1nm() // Nv1 (nautiques x 10)
{
	HTTPClient http;
	http.begin(srvName_vor_nm);
	int httpResponseCode = http.GET();
	if (httpResponseCode>0)	{ recp_vor1nm = http.getString();	}

	//Serial.println(recp_vor1nm);
	
	http.end();
}


void httpGetILSnm() // Nv2(ILS) (nautiques x 10)
{
	HTTPClient http;
	http.begin(srvName_ils_nm);
	int httpResponseCode = http.GET();
	if (httpResponseCode>0)	{ recp_ILSnm = http.getString();	}

	//Serial.println(recp_ILS1nm);
	
	http.end();
}



void httpGetILSactual() // ILS actual (direction de l'avion vu depuis l'ILS)
{
	HTTPClient http;
	http.begin(srvName_ils_actual);
	int httpResponseCode = http.GET();
	if (httpResponseCode>0)	{ recp_ILS_actual = http.getString();	}
	http.end();
}


/*

void httpGet_SW() // depuis le SW: c.a.d l'ESP32 n°3
{
	HTTPClient http;
	http.begin(srvName_sw);
	int httpResponseCode = http.GET();
	if (httpResponseCode>0)	{ switches = http.getString();	}
	http.end();
}
*/


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

après les décalages on obtient les 16 bits suivants:

rrrrr---========
     gggggg--===
        ===bbbbb

soit après le ou :

rrrrrggggggbbbbb

calcul de la Fonction inverse : 
RGB565_to_888
**/

uint16_t Color_To_565(uint8_t r, uint8_t g, uint8_t b)
{
	return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
}


void RGB565_to_888(uint16_t color565, uint8_t *R, uint8_t *G, uint8_t *B)
{
	*R=(color565 & 0xFFFFF800) >> 8;
	*G=(color565 & 0x7E0) >> 3;
	*B=(color565 & 0x1F) << 3 ;
}




uint16_t bmp_width;
uint16_t bmp_heigh;


uint16_t read_16(File fp)
{
    uint8_t low;
    uint16_t high;
    low = fp.read();
    high = fp.read();
    return (high<<8)|low;
}



uint32_t read_32(File fp)
{
    uint16_t low;
    uint32_t high;
    low = read_16(fp);
    high = read_16(fp);
    return (high<<8)|low;   
}



void write_16(uint16_t v16, File fp)
{
    uint8_t low, high;
	
	low = v16 & 0xFF;
	high= v16 >>8;
    
    fp.write(low);
    fp.write(high);
}


void write_32(uint32_t v32, File fp)
{
    uint16_t low, high;
	
	low = v32 & 0xFFFF;
	high= v32 >>16;
    
    write_16(low, fp);
    write_16(high, fp);
}



bool teste_bmp_header(File fp)
{
    if(read_16(fp) != 0x4D42)  { return false;  } // (2 bytes) The header field used to identify the BMP
    read_32(fp); // (4 bytes) get bmp size (nombre total d'octets)
    read_32(fp); // (4 bytes) get creator information
    bmp_offset = read_32(fp); // (4 bytes) get offset information
    read_32(fp);//get DIB information

// ici on a lu 16 octets  
	bmp_width = read_32(fp); //(4 bytes) get width and heigh information
	bmp_heigh = read_32(fp); //(4 bytes)
	
// ici on a lu 24 octets 	
    //if(read_16(fp)!= 1) {return false;}
    read_16(fp);
    //if(read_32(fp)!= 0) {return false;}
    return true;
}




void draw_bmp(uint16_t x0, uint16_t y0, File* fp)
{

	//sram = freeRam(); Serial.print("03-freeRam="); Serial.println(sram);
		uint16_t i,j,k,p,m=0;
		uint8_t bmp_data[2*3]={0};
		uint16_t bmp_color[2];
		
		fp->seek(bmp_offset);
		for(i=0; i<bmp_heigh; i++)
		{
			for(j=0; j<(bmp_width/2); j++)
			{
				m=0;
				fp->read(bmp_data,2*3);
				for(k=0; k<2; k++)
				{
					//bmp_color[k]= Color_To_565(bmp_data[m+2], bmp_data[m+1], bmp_data[m+0]);
					bmp_color[k]= Color_To_565(bmp_data[m+2], bmp_data[m+1], bmp_data[m+0]);
					m+=3;
				}
				for(p=0; p<2; p++)
				{
					//TFT480.drawPixel(x0+i, y0+j*2+p, bmp_color[p]); // si rotation 0
					TFT480.drawPixel(x0+j*2+p, 320 - (y0+i), bmp_color[p]); // si rotation 1
				}   
			}
		}
	
}




void affi_img(uint16_t x0, uint16_t y0, const char* filename1)
{

	File bmp_file;
	TFT480.setFreeFont(FF1);
	TFT480.setTextColor(ORANGE, NOIR);
	bmp_file = SD.open(filename1);
	if(!bmp_file)
	{
		//TFT480.drawString("didnt find bmp", 260, 20);
		//delay(100);
		//TFT480.fillRect(260,20, 180, 20, NOIR); // efface
		efface_carte();
		bmp_file.close(); 
		return;
	}
	if(!teste_bmp_header(bmp_file))
	{
		//TFT480.drawString("bad bmp header !", 260, 20);
		//delay(100);
		//FT480.fillRect(260,20, 180, 20, NOIR); // efface
		bmp_file.close(); 
		return;
	}
	draw_bmp(x0, y0, &bmp_file);

	bmp_file.close(); 
//	delay(1000);

}

/** -----------------------------------------------------------------------------------
		CAPTURE D'ECRAN vers SDcard				
/** ----------------------------------------------------------------------------------- */

void write_TFT480_on_SDcard() // enregistre le fichier .bmp
{
	if (SDcardOk==1)
	{
		////TFT480.drawString("capture bmp !", 100, 100);
		////delay(1000);
		
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

			
			TFT480.setTextColor(VERT, NOIR);
			
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
}

/** ----------------------------------------------------------------------------------- */



void Draw_arc_elliptique(uint16_t x0, uint16_t y0, int16_t dx, int16_t dy, float angle1, float angle2, uint16_t couleur)
{
/*
REMARQUES :	
-cette fonction permet également de dessiner un arc de cercle (si dx=dy), voire le cercle complet
- dx et dy sont du type int (et pas uint) et peuvent êtres négafifs, ou nuls.
-alpha1 et alpha2 sont les angles (en radians) des caps des extrémités de l'arc 
*/

// angle1 et angle2 en degrés décimaux

 
	float alpha1 =angle1/57.3;  // (57.3 ~ 180/pi)
	float alpha2 =angle2/57.3; 
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


void affi_rayon1(uint16_t x0, uint16_t y0, uint16_t rayon, float angle_i, float pourcent, uint16_t couleur_i, bool gras)
{
// trace une portion de rayon de cercle depuis 100%...à pourcent du rayon du cercle
// angle_i en degrés décimaux - sens trigo

	float angle =angle_i/57.3;  // (57.3 ~ 180/pi)
	float x1, x2;
	float y1, y2;

	x1=x0+rayon* cos(angle);
	y1=y0-rayon* sin(angle);

	x2=x0+pourcent*rayon* cos(angle);
	y2=y0-pourcent*rayon* sin(angle);

	TFT480.drawLine(x1, y1, x2, y2, couleur_i);

/*	
	if (gras)
	{
		TFT480.drawLine(x1, y1-1, x2, y2-1, couleur_i);
		TFT480.drawLine(x1, y1+1, x2, y2+1, couleur_i);
	}
*/	
}



void affi_rayon2(uint16_t x0, uint16_t y0, float r1, float r2, float angle_i, uint16_t couleur_i, uint8_t gras)
{
// trace une portion de rayon de cercle entre les distances r1 et r2 du centre
// angle_i en degrés décimaux - sens trigo (= anti-horaire)
// origine d'angle -> trigo (à droite)

	float angle =angle_i/57.3;  // (57.3 ~ 180/pi)
	int16_t x1, x2;
	int16_t y1, y2;

	
	x1=x0+int16_t(r1* cos(angle));
	y1=y0-int16_t(r1* sin(angle));

	x2=x0+int16_t(r2* cos(angle));
	y2=y0-int16_t(r2* sin(angle));

//en fait si les extrémités dépassent de la taille de l'afficheur, ça se passe bien !
	//if ((x1>0) && (x2>0) && (y1>0) && (y2>0) && (x1<480) && (x2<480) && (y1<320) && (y2<320) )
	{
		TFT480.drawLine(x1, y1, x2, y2, couleur_i);
		
		if (gras==1)
		{
			TFT480.drawLine(x1, y1-1, x2, y2-1, couleur_i);
			TFT480.drawLine(x1, y1+1, x2, y2+1, couleur_i);
		}
		
	}
	
}


void affi_SPR_rayon2(uint16_t x0, uint16_t y0, float r1, float r2, float angle_i, uint16_t couleur_i, bool gras)
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
		SPR_VOR.drawLine(x1, y1, x2, y2, couleur_i);
		
		if (gras)
		{
			SPR_VOR.drawLine(x1, y1-1, x2, y2-1, couleur_i);
			SPR_VOR.drawLine(x1, y1+1, x2, y2+1, couleur_i);
		}
	}
}



void affi_tiret_H(uint16_t x0, uint16_t y0, uint16_t r, float angle_i, uint16_t couleur_i)
{
// trace un tiret perpendiculaire à un rayon de cercle de rayon r
// angle_i en degrés décimaux - sens trigo

	float angle =angle_i/57.3;  // (57.3 ~ 180/pi)
	float x1, x2;
	float y1, y2;

	x1=x0+(r)* cos(angle-1);
	y1=y0-(r)* sin(angle-1);		

	x2=x0+(r)* cos(angle+1);
	y2=y0-(r)* sin(angle+1);

	TFT480.drawLine(x1, y1,  x2, y2, couleur_i);	
}



void affi_pointe(uint16_t x0, uint16_t y0, uint16_t r, float angle_i, uint16_t couleur_i)
{
// trace une pointe de flèche sur un cercle de rayon r
// angle_i en degrés décimaux - sens trigo

	float angle = angle_i /57.3;  // (57.3 ~ 180/pi)
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



void affi_SPR_encoche(uint16_t x0, uint16_t y0, uint16_t r, float angle_i, uint16_t couleur_i)
{
// trace une encoche (deux 'demi'-triangles) sur un cercle de rayon r
// angle_i en degrés décimaux - sens trigo

	float angle = angle_i /57.3;  // (57.3 ~ 180/pi)
	int16_t x1, x2, x3;
	int16_t y1, y2, y3;


// partie gauche
	x1=x0+r* cos(angle+0.08); // pointe
	y1=y0-r* sin(angle+0.08); // pointe

	x2=x0+(r-10)* cos(angle+0.08); // base A
	y2=y0-(r-10)* sin(angle+0.08); // base A		

	x3=x0+(r-10)* cos(angle); // base B
	y3=y0-(r-10)* sin(angle); // base B

	SPR_VOR.fillTriangle(x1, y1,  x2, y2,  x3, y3,	couleur_i);

// partie droite
	x1=x0+r* cos(angle-0.08); // pointe
	y1=y0-r* sin(angle-0.08); // pointe

	x2=x0+(r-10)* cos(angle-0.08); // base A
	y2=y0-(r-10)* sin(angle-0.08); // base A		

	x3=x0+(r-10)* cos(angle); // base B
	y3=y0-(r-10)* sin(angle); // base B

	SPR_VOR.fillTriangle(x1, y1,  x2, y2,  x3, y3,	couleur_i);
}



void affi_rectangle_incline(uint16_t x0, uint16_t y0, uint16_t r, float angle_i, uint16_t couleur_i)
{
//rectangle inscrit dans le cerce de rayon r
// angle_i en degrés décimaux - sens trigo

	float angle =(angle_i-3.5)/57.3;
	int16_t x1, x2, x3, x4;
	int16_t y1, y2, y3, y4;
	float d_alpha=0.05; // détermine la largeur du rectangle

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


void affi_dst_VOR() // distance de la balise vue de l'avion
{

	if (vor_dst<0) {vor_dst=0;}
	String s1;
	uint16_t x0=5;
	uint16_t y0=260; 
	float nav_nm;
// rappel: 1 mile marin (NM nautical mile) = 1852m

	nav_nm = vor_dst / 10.0;
	if(nav_nm>999){nav_nm=0;} // pour éviter un affichage fantaisiste si le PFD n'est pas en marche
	TFT480.setFreeFont(FM9);
	TFT480.setTextColor(GRIS_CLAIR, NOIR);
	TFT480.drawString("VOR", x0, y0);
	TFT480.setTextColor(BLANC, NOIR);

	uint8_t nb_decimales =1;
	if (nav_nm >99) {nb_decimales=0;}
	s1 = String( nav_nm, nb_decimales);
	
	TFT480.fillRect(x0+40, y0, 52, 18, NOIR); // efface
	TFT480.drawString(s1, x0+40, y0);
	TFT480.drawRoundRect(x0+40, y0-2, 50, 18, 5, GRIS_FONCE); // encadrement de la valeur affichée

	TFT480.drawString("NM", x0+95, y0);
}


void affi_dst_ILS() // distance de la balise vue de l'avion
{

	if (ils_dst<0) {ils_dst=0;}
	String s1;
	uint16_t x0=5;
	uint16_t y0=280;
	
	// rappel: 1 mile marin (NM nautical mile) = 1852m

	ils_nm = ils_dst / 10.0;
	if(ils_nm>999){ils_nm=0;} // pour éviter un affichage fantaisiste si le PFD n'est pas en marche
	TFT480.setFreeFont(FM9);
	TFT480.setTextColor(GRIS_CLAIR, NOIR);
	TFT480.drawString("ILS", x0, y0);
	TFT480.setTextColor(BLANC, NOIR);

	uint8_t nb_decimales =1;
	if (ils_nm >99) {nb_decimales=0;}
	s1 = String( ils_nm, nb_decimales);
	
	TFT480.fillRect(x0+40, y0, 52, 18, NOIR); // efface
	TFT480.drawString(s1, x0+40, y0);
	TFT480.drawRoundRect(x0+40, y0-2, 50, 18, 5, GRIS_FONCE); // encadrement de la valeur affichée

	TFT480.drawString("NM", x0+95, y0);
}


void affi_radial_VOR() // concerne le VOR : direction de la balise vue de l'avion
{

	if (vor_radial<0) {vor_radial=0;}
	String s;
	uint16_t x0=133;
	uint16_t y0=260; 

	int16_t alpha1 = vor_radial;
	
	if ((alpha1<0)||(alpha1 >360)) {alpha1 =0;}
	int16_t alpha2 = alpha1 + 180;	if (alpha2>360)  {alpha2 -= 360;}
	
	
//affichage numérique de l'angle
	TFT480.setFreeFont(FM9);
	TFT480.setTextColor(VERT, NOIR);
	s = (String) alpha1;
	TFT480.fillRect(x0, y0, 50, 18, NOIR); // efface
	TFT480.setFreeFont(FM9);
	TFT480.drawString(s, x0+5, y0);
	TFT480.drawRoundRect(x0, y0-2, 40, 18, 5, GRIS_FONCE); // encadrement de la valeur affichée
	//TFT480.drawCircle(x0+46, y0, 2, JAUNE); // caractère 'degré'

//affichage numérique de l'angle opposé
	x0+=42;
	TFT480.setTextColor(VERT, NOIR);
	s = (String) alpha2;
	TFT480.fillRect(x0, y0, 50, 18, NOIR); // efface
	TFT480.setFreeFont(FM9);
	TFT480.drawString(s, x0+5, y0);
	TFT480.drawRoundRect(x0, y0-2, 42, 18, 5, GRIS_FONCE); // encadrement de la valeur affichée
	TFT480.drawCircle(x0+46, y0, 2, JAUNE);

// au dessus du grand cercle :
	cadre_rd1.efface();
	cadre_rd1.affiche_int(alpha2);
}



void affi_actual_ILS() // direction de la balise ILS vue de l'avion; ne pas confondre avec radial_ILS (orientation fixe du faisceau ILS)
{

	//TFT480.drawCircle(240, 160, 100, JAUNE); // pour test

	if (ils_actual<0) {ils_actual=0;}
	String s;
	uint16_t x0=133;
	uint16_t y0=280; 

	int16_t alpha1 = ils_actual;
	
	if ((alpha1<0)||(alpha1 >360)) {alpha1 =0;}
	int16_t alpha2 = alpha1 + 180;
	if (alpha2>360)  {alpha2 -= 360;}
	
	
//affichage numérique de l'angle
	TFT480.setFreeFont(FM9);
	TFT480.setTextColor(VERT, NOIR);
	s = (String) alpha1;
	TFT480.fillRect(x0, y0, 50, 18, NOIR); // efface
	TFT480.setFreeFont(FM9);
	TFT480.drawString(s, x0+5, y0);
	TFT480.drawRoundRect(x0, y0-2, 40, 18, 5, GRIS_FONCE); // encadrement de la valeur affichée
	TFT480.drawCircle(x0+46, y0, 2, JAUNE); // caractère 'degré'

//affichage numérique de l'angle opposé
	x0+=42;
	TFT480.setTextColor(VERT, NOIR);
	s = (String) alpha2;
	TFT480.fillRect(x0, y0, 50, 18, NOIR); // efface
	TFT480.setFreeFont(FM9);
	TFT480.drawString(s, x0+5, y0);
	TFT480.drawRoundRect(x0, y0-2, 40, 18, 5, GRIS_FONCE); // encadrement de la valeur affichée
	TFT480.drawCircle(x0+46, y0, 2, JAUNE);


	if ((ils_nm >0) && (ils_nm<200))
	{
		float R = ils_nm * px_par_NM;
		x_avion=357 + R * cos((90+alpha2)/57.3);
		y_avion=75  + R * sin((90+alpha2)/57.3);		
	}
	else
	{
		x_avion=0;
		y_avion=0;
	}
}



void rotation1()
{
// acquisition de l'encodeur pas à pas (1)	
// selection balise

	if ( millis() - TEMPO  >= timer1 )
	{
		timer1 = millis();
		bool etat = digitalRead(rot1b);
		if(etat == 0) {num_bali++;} else {num_bali--;}
		if (num_bali<0){num_bali=0;}
		if (num_bali > nb_elements-1){num_bali = nb_elements-1;}
		zoom=2;
		refresh_ND=1; // pour raffraichir l'affichage du panneau de fréquences à droite
		refresh_Encoche=1;
		refresh_carte=1;
	}
}



void rotation2()
{
// acquisition de l'encodeur pas à pas (2)
// zoom sur la carte

	if ( millis() - TEMPO  >= timer2 )
	{
		timer2 = millis();
		bool etat = digitalRead(rot2b);
		if(etat == 0) {zoom--;} else {zoom++;}
		if (zoom<1){zoom=1;}
		if (zoom > 11){zoom = zoom=11;}
		refresh_carte=01; // pour raffraichir l'affichage du panneau de fréquences à droite

	}
}



void init_SDcard()
{
	String s1;
	
	TFT480.fillRect(0, 0, 480, 320, NOIR); // efface
	TFT480.setTextColor(BLANC, NOIR);
	TFT480.setFreeFont(FF1);

	uint16_t y=0;
	
	TFT480.drawString("ND - Navigation Display", 0, y);
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



void dessine_avion(uint16_t x, uint16_t y) // celui, fixe,  au centre du grand cercle
{
	TFT480.fillRoundRect(x+3, y-5, 6, 25, 2, BLEU); // fuselage
	TFT480.fillRoundRect(x-9, y, 30, 6, 2, BLEU);   // aile
	TFT480.fillRoundRect(x, y+20, 13, 4, 2, BLEU);  // stab
}





void affi_avion_sur_carte(float alpha) // dessine le sprite du petit sur la carte, mobile, avec l'orientation alpha
{
	//TFT480.drawRect(x-5, y-3, 15, 12, NOIR); // pour test visu délimitation de la zone

	if (memo_x_avion !=0)
	{
		TFT480.pushRect(memo_x_avion-10, memo_y_avion-10, 20, 20, memo_img); // print (petit) fond de carte
	}
	
	if ((x_avion > 245) && (y_avion > 10) && (y_avion < 170))
	{
		TFT480.readRect(x_avion-10, y_avion-10, 20, 20, memo_img); // enregistre en mémoire RAM
		
	//affiche le sprite de l'avion avec la bonne orientation

		TFT480.setPivot(x_avion, y_avion);

		SPR_avion.pushRotated(alpha, TFT_BLACK); // noir définit comme transparent

		memo_x_avion = x_avion;
		memo_y_avion = y_avion;
	}
}




void init_sprites()
{
	// sprites représentant les lettres 'N' 'S' 'E' 'O' qui seront affichées  sur un cercle, inclinées donc.


	SPR_E.setFreeFont(FF5); 	SPR_E.setTextColor(JAUNE);
	SPR_E.createSprite(SPR_W, SPR_H);
	SPR_E.setPivot(SPR_W/2, SPR_H/2);  // Set pivot relative to top left corner of Sprite
	SPR_E.fillSprite(NOIR);
	SPR_E.drawString("E", 2, 2 );

	SPR_N.setFreeFont(FF5); 	SPR_N.setTextColor(JAUNE);
	SPR_N.createSprite(SPR_W, SPR_H);
	SPR_N.setPivot(SPR_W/2, SPR_H/2);
	SPR_N.fillSprite(NOIR);
	SPR_N.drawString("N", 2, 2 );

	SPR_O.setFreeFont(FF5); 	SPR_O.setTextColor(JAUNE);
	SPR_O.createSprite(SPR_W, SPR_H);
	SPR_O.setPivot(SPR_W/2, SPR_H/2);
	SPR_O.fillSprite(NOIR);
	SPR_O.drawString("W", 2, 2 );

	SPR_S.setFreeFont(FF5); 	SPR_S.setTextColor(JAUNE);
	SPR_S.createSprite(SPR_W, SPR_H);
	SPR_S.setPivot(SPR_W/2, SPR_H/2);
	SPR_S.fillSprite(NOIR);
	SPR_S.drawString("S", 2, 2 );

// sprites représentant les nombres '3' '6' '12' '15' '21' '24' '30' '33' 

	SPR_3.setFreeFont(FF5); 	SPR_3.setTextColor(BLANC);
	SPR_3.createSprite(SPR_W, SPR_H);
	SPR_3.setPivot(SPR_W/2, SPR_H/2);  // Set pivot relative to top left corner of Sprite
	SPR_3.fillSprite(NOIR);
	SPR_3.drawString("3", 2, 2 );

	SPR_6.setFreeFont(FF5); 	SPR_6.setTextColor(BLANC);
	SPR_6.createSprite(SPR_W, SPR_H);
	SPR_6.setPivot(SPR_W/2, SPR_H/2);
	SPR_6.fillSprite(NOIR);
	SPR_6.drawString("6", 2, 2 );

	SPR_12.setFreeFont(FF5); 	SPR_12.setTextColor(BLANC);
	SPR_12.createSprite(SPR_W, SPR_H);
	SPR_12.setPivot(SPR_W/2, SPR_H/2);
	SPR_12.fillSprite(NOIR);
	SPR_12.drawString("12", 2, 2 );

	SPR_15.setFreeFont(FF5); 	SPR_15.setTextColor(BLANC);
	SPR_15.createSprite(SPR_W, SPR_H);
	SPR_15.setPivot(SPR_W/2, SPR_H/2);
	SPR_15.fillSprite(NOIR);
	SPR_15.drawString("15", 2, 2 );

	SPR_21.setFreeFont(FF5); 	SPR_21.setTextColor(BLANC);
	SPR_21.createSprite(SPR_W, SPR_H);
	SPR_21.setPivot(SPR_W/2, SPR_H/2);  // Set pivot relative to top left corner of Sprite
	SPR_21.fillSprite(NOIR);
	SPR_21.drawString("21", 2, 2 );

	SPR_24.setFreeFont(FF5); 	SPR_24.setTextColor(BLANC);
	SPR_24.createSprite(SPR_W, SPR_H);
	SPR_24.setPivot(SPR_W/2, SPR_H/2);
	SPR_24.fillSprite(NOIR);
	SPR_24.drawString("24", 2, 2 );

	SPR_30.setFreeFont(FF5); 	SPR_30.setTextColor(BLANC);
	SPR_30.createSprite(SPR_W, SPR_H);
	SPR_30.setPivot(SPR_W/2, SPR_H/2);
	SPR_30.fillSprite(NOIR);
	SPR_30.drawString("30", 2, 2 );

	SPR_33.setFreeFont(FF5); 	SPR_33.setTextColor(BLANC);
	SPR_33.createSprite(SPR_W, SPR_H);
	SPR_33.setPivot(SPR_W/2, SPR_H/2);
	SPR_33.fillSprite(NOIR);
	SPR_33.drawString("33", 2, 2 );

	SPR_VOR.setFreeFont(FF5); 	SPR_33.setTextColor(BLANC);
	SPR_VOR.createSprite(180, 180);
	SPR_VOR.setPivot(90, 90);
	SPR_VOR.fillSprite(NOIR);  // pour test en rotation --> BLEU

	//SPR_avion.setFreeFont(FF5); 	SPR_33.setTextColor(BLANC);
	SPR_avion.createSprite(20, 20);
	SPR_avion.setPivot(10, 10);
	//SPR_avion.fillSprite(NOIR);  // pour test en rotation --> BLEU

//--------------------------------------------------------------	
//		DESSIN DU COMPAS gradué
//--------------------------------------------------------------
// dessine les tirets de graduation du compas sur le sprite SPR_VOR

	uint16_t angle1 = 0;
	uint8_t a;
	uint16_t R =90;
	for(uint8_t n=0; n<72; n+=2 )
	{
		angle1 = n*5;  // 1 tiret tous les 15 degrés
		a= 3;
		//if ((n%2) == 0){a=10;}
		if ((n%6) == 0){a=12;}
		affi_SPR_rayon2(90, 90, R-a, R, angle1, BLANC, false);
	}
//--------------------------------------------------------------
// écrit les nombres (sprites définis ci-dessus) sur le sprite SPR_VOR

	uint16_t x_spr;
	uint16_t y_spr;


	x_spr = 90 +70*cos(degTOrad(-60));
	y_spr = 90 -70*sin(degTOrad(-60));
	SPR_VOR.setPivot(x_spr, y_spr);
	SPR_15.pushRotated(&SPR_VOR, 150); // sur adresse relative du SPR_VOR


	x_spr = 90 +70*cos(degTOrad(-30));
	y_spr = 90 -70*sin(degTOrad(-30));
	SPR_VOR.setPivot(x_spr, y_spr);
	SPR_12.pushRotated(&SPR_VOR, 120);
	
	
	x_spr = 90 +70*cos(degTOrad(0-5));
	y_spr = 90 -70*sin(degTOrad(0-5));
	SPR_VOR.setPivot(x_spr, y_spr);
	SPR_E.pushRotated(&SPR_VOR, 90); // Plot rotated Sprite 'SPR_E' dans le sprite 'SPR_VOR'


	x_spr = 90 +70*cos(degTOrad(30-5));
	y_spr = 90 -70*sin(degTOrad(30-5));
	SPR_VOR.setPivot(x_spr, y_spr);
	SPR_6.pushRotated(&SPR_VOR, 60);
	

	x_spr = 90 +70*cos(degTOrad(60-5));
	y_spr = 90 -70*sin(degTOrad(60-5));
	SPR_VOR.setPivot(x_spr, y_spr);
	SPR_3.pushRotated(&SPR_VOR, 30);

	x_spr = 90 +70*cos(degTOrad(90-5));
	y_spr = 90 -70*sin(degTOrad(90-5));
	SPR_VOR.setPivot(x_spr, y_spr);
	SPR_N.pushRotated(&SPR_VOR, 0);

	x_spr = 90 +70*cos(degTOrad(120));
	y_spr = 90 -70*sin(degTOrad(120));
	SPR_VOR.setPivot(x_spr, y_spr);
	SPR_33.pushRotated(&SPR_VOR, -30);	

	x_spr = 90 +70*cos(degTOrad(150));
	y_spr = 90 -70*sin(degTOrad(150));
	SPR_VOR.setPivot(x_spr, y_spr);
	SPR_30.pushRotated(&SPR_VOR, -60);	
	
	x_spr = 90 +70*cos(degTOrad(180-5));
	y_spr = 90 -70*sin(degTOrad(180-5));
	SPR_VOR.setPivot(x_spr, y_spr);
	SPR_O.pushRotated(&SPR_VOR, -90);

	x_spr = 90 +70*cos(degTOrad(210));
	y_spr = 90 -70*sin(degTOrad(210));
	SPR_VOR.setPivot(x_spr, y_spr);
	SPR_24.pushRotated(&SPR_VOR, 240);
	
	x_spr = 90 +70*cos(degTOrad(240));
	y_spr = 90 -70*sin(degTOrad(240));
	SPR_VOR.setPivot(x_spr, y_spr);
	SPR_21.pushRotated(&SPR_VOR, 210);
	

	x_spr = 90 +70*cos(degTOrad(-90-5));
	y_spr = 90 -70*sin(degTOrad(-90-5));
	SPR_VOR.setPivot(x_spr, y_spr);
	SPR_S.pushRotated(&SPR_VOR, 180);
	

//--------------------------------------------------------------
// dessin du petit avion dans son sprite pour affichage sur la carte (orientaion nulle ici)

	//SPR_avion.fillRect(0,0,19,19, VERT);  // pour test visuel du cadrage du dessin dans le sprite
	SPR_avion.fillRoundRect(7+1, 8-3, 3, 12, 2, BLEU); // fuselage
	SPR_avion.fillRoundRect(7-5, 8, 15, 3, 2, BLEU);   // aile
	SPR_avion.fillRoundRect(7-1, 8+8, 7, 2, 2, BLEU);  // stab
}



void init_affi_vor()
{

	uint16_t x0 = 120;
	uint16_t y0 = 120;
	uint16_t w = 240;
	uint16_t h = 240;
	
	uint16_t R =100;
	uint16_t R2 =70;
	
	//TFT480.fillRect(x0-w/2, y0-h/2, w, h, NOIR);
	TFT480.drawRoundRect(0, 0, 230, 240, 5, GRIS_FONCE);

	uint16_t y1 = y0+12;

	////TFT480.fillTriangle(
	////x0, y1+18-h/2,
	////x0-5, y1+12-h/2,
	////x0+5, y1+12-h/2,
	////BLANC);	
	
	//TFT480.fillCircle(x0, y0, R, NOIR);
	//TFT480.fillCircle(x0, y1, 5, GRIS_FONCE);
	//TFT480.drawCircle(x0, y1, R, GRIS_FONCE);

	dessine_avion(104, 125);
	
}


void affi_index_cap() // petite pointe de flèche au sommet du grand cercle
{
	uint16_t x0 = 110;
	uint16_t y0 = 120+5;
	uint16_t h = 180;
	TFT480.fillTriangle(
	x0, y0+6-h/2,
	x0-5, y0-h/2,
	x0+5, y0-h/2,
	BLANC);
	TFT480.setFreeFont(FM9);
	TFT480.setTextColor(BLANC, NOIR);
	//TFT480.drawString("HDG", x0-60, y0-30-h/2);
	
}



void affi_encoche_hdg(uint16_t alpha_i, uint16_t couleur)  // encoche qui se déplace sur le grand sprite
{

	init_sprites();
	affi_SPR_encoche(90, 90, 90, -alpha_i +90, couleur);
	
	//affi_rayon2(x0, y0, 85, 20, -alpha_i, couleur, false);
}




void affi_ND(float alpha_i)  // Navigation Display (le grand cercle à gauche avec les différent affichages dessus)
{

	uint16_t w = 180;
	uint16_t h = 180;
	
	uint16_t R =100;
	uint16_t R2 =70;
	
	float angle1, angle2;

	uint16_t x_spr;
	uint16_t y_spr;


	////SPR_VOR.fillRect(0, 0, 200, 200, NOIR);

	////angle1 = 0;
	////uint8_t a;
	////for(uint8_t n=0; n<72; n+=2 )
	////{
		////angle1 = cap_i + n*5;  // 1 tiret tous les 15 degrés
		////a= 5;
		//////if ((n%2) == 0){a=10;}
		////if ((n%6) == 0){a=15;}
		////affi_SPR_rayon2(x0, y1, R-a, R, degTOrad(angle1), couleur, false);
	////}

	angle2 = -alpha_i;


	//SPR_VOR.pushSprite(0, 0);

	TFT480.setPivot(110, 130);
	SPR_VOR.setPivot(90, 90);
	
	SPR_VOR.pushRotated(angle2); // affiche le grand cercle sous forme d'un (grand) sprite
	dessine_avion(104, 125);


	TFT480.drawRoundRect(0, 0, 230, 240, 5, GRIS_FONCE);


	affi_index_cap(); // petite pointe de flèche au sommet du grand cercle


	cadre_cap1.efface();
	cadre_cap1.affiche_int(cap);


// rectangle visualisant l'orientation de la piste = ILS = Nav2 (= Nav[1] dans la liste des propriétés internes)
	float angle3 = 90.0+ils_orientation/100; // en degrés décimaux
	affi_rectangle_incline(110, 130, 60, angle3 + angle2 +3, BLEU_CLAIR);

	
// flêche -> direction/Nord du VOR, vue de l'avion

	int16_t alpha2 = vor_radial + 180;
	if (alpha2>360)  {alpha2 -= 360;}

	
	affi_rayon2(110, 130, 70, 20, -angle2 -alpha2 +90, GRIS_CLAIR, 1); // vers Nav1 radial = VOR
	affi_rayon2(110, 130, 70, 20, -angle2 -vor_radial +90, GRIS_FONCE, 0);  // azimut de l'avion vu du VOR
	
	affi_pointe(110, 130, 70, -angle2 -alpha2 +90, GRIS_CLAIR); // pointe dans la direction du VOR

// HDG	
	affi_rayon2(110, 130, 80, 20, -angle2 -hdg1 +90, OLIVE, 1); // HDG

	
	

}



void dessine_pannel_frq()
{
	uint16_t x0 = 235;
	uint16_t y0 = 172;
	uint16_t xi, yi;

	xi = x0+5;
	yi = y0+5;

	TFT480.drawRoundRect(x0, y0, 245, 146, 5, GRIS);

	yi = y0+5;

// -------------------------------------
// entête

	cadre_top1.init(xi,yi,60,22);
	cadre_top1.couleur_contour = GRIS_FONCE;
	cadre_top1.couleur_texte = BLANC;
	cadre_top1.traceContour();
	yi=y0+30;

	cadre_top2.init(xi,yi,220,22);
	cadre_top2.couleur_contour = GRIS_FONCE;
	cadre_top2.couleur_texte = BLANC;
	cadre_top2.traceContour();


// -------------------------------------
// colonne 0

	yi=y0+55;

	TFT480.setFreeFont(FM9);	TFT480.setTextColor(GRIS_CLAIR, NOIR);
	TFT480.drawString("VOR", xi+5, yi+5);

	cadre_00.init(xi+45,yi,75,22);
	cadre_00.couleur_contour = GRIS_FONCE;	cadre_00.couleur_texte = VERT;
	cadre_00.traceContour();
	yi+=25;

	
	TFT480.setFreeFont(FM9);	TFT480.setTextColor(BLEU_CLAIR, NOIR);
	TFT480.drawString("ILS", xi+5, yi+5);

	cadre_01.init(xi+45,yi,75,22);
	cadre_01.couleur_contour = GRIS_FONCE;	cadre_01.couleur_texte = VERT;
	cadre_01.traceContour();
	yi+=25;
	

// -------------------------------------
// colonne 1

	xi+=80;
	yi=y0+55;

	cadre_10.init(xi+45,yi,50,22);
	cadre_10.couleur_contour = GRIS_FONCE;	cadre_10.couleur_texte = GRIS_CLAIR;
	cadre_10.traceContour();
	yi+=25;

	cadre_11.init(xi+45,yi,50,22);
	cadre_11.couleur_contour = GRIS_FONCE;	cadre_11.couleur_texte = VERT;
	cadre_11.traceContour();
	yi+=25;
	

	// -------------------------------------
// colonne 2

	xi+=55;
	yi=y0+55;

	cadre_20.init(xi+45,yi,40,22);
	cadre_20.couleur_contour = GRIS_FONCE;	cadre_20.couleur_texte = GRIS_CLAIR;
	cadre_20.traceContour();
	yi+=25;


	cadre_21.init(xi+45,yi,40,22);
	cadre_21.couleur_contour = GRIS_FONCE;	cadre_21.couleur_texte = BLEU_CLAIR;
	cadre_21.traceContour();
	yi+=25;

// -------------------------------------
// pied

	xi=x0+5;

	cadre_bas1.init(xi,yi,105,22);
	cadre_bas1.couleur_contour = GRIS_FONCE;
	cadre_bas1.couleur_texte = JAUNE;
	cadre_bas1.traceContour();

	xi+=110;
	cadre_bas2.init(xi,yi,110,22);
	cadre_bas2.couleur_contour = GRIS_FONCE;
	cadre_bas2.couleur_texte = ROUGE;
	cadre_bas2.traceContour();

	
	
// -------------------------------------
// 3 cadres au dessus du grand cercle :

	cadre_hdg1.init(20,3,40,22);
	cadre_hdg1.couleur_contour = GRIS;
	cadre_hdg1.couleur_texte = OLIVE;
	cadre_hdg1.traceContour();

	TFT480.setTextFont(2);
	TFT480.setTextColor(VERT, NOIR);
	TFT480.drawString("HDG", 10, 27);	

	cadre_cap1.init(100,3,40,22);
	cadre_cap1.couleur_contour = GRIS;
	cadre_cap1.couleur_texte = BLANC;
	cadre_cap1.traceContour();

	cadre_rd1.init(180,3,40,22);
	cadre_rd1.couleur_contour = GRIS;
	cadre_rd1.couleur_texte = GRIS_CLAIR;
	cadre_rd1.traceContour();

	TFT480.setTextFont(2);
	TFT480.setTextColor(GRIS_CLAIR, NOIR);
	TFT480.drawString("VOR", 200, 27);




}



void efface_carte()
{
	uint16_t x0 = 235;
	uint16_t y0 = 0;
	TFT480.fillRoundRect(x0, y0, 245, 170, 5, PAMPA);
}


/**
 ECHELLES des cartes OpenStreetMap

pour les petites cartes 240x160 px:
N°
0.bmp	500m->70px	= 70/0.5 soit 140 px/km
1.bmp	1km->70px	= 70/1	soit  70 px/km 
2.bmp	2km->70px	= 70/2	soit  35 px/km
3.bmp	5km->87px	= 87/5	soit  17.5 px/km 
4.bmp	10km->87px	= 87/10	soit  8.75 px/km 
5.bmp	20km->87px	= 87/20 soit  4.37 px/km 
6.bmp	30km->66px	= 65/30 soit  2.19 px/km 

On voit que la progression est /2 depuis 140

**/

void affi_carte()
{
	uint16_t x0 = 235;
	uint16_t y0 = 0;
	String s1;
	char var_array[25];
	
	
	TFT480.drawRoundRect(x0, y0, 245, 170, 5, GRIS);


	s1="/bmp/";
	s1 +=  liste_bali[num_bali].ID_OACI;
	s1 +=  "/240x160/";
	s1 += String(zoom);
	s1+= ".bmp";
	s1+='\0';

	for(int i=0; i<24; i++)	{var_array[i] = s1[i];}
	var_array[24]=0;  // zéro terminal  -> string

	affi_img(237,156, var_array);

	TFT480.setFreeFont(FM9);
	TFT480.setTextColor(NOIR);
	s1=(String) (zoom);
	s1+= ".bmp";
	TFT480.fillRect(x0+185, y0+5, 50, 10, BLANC); //efface
	TFT480.drawString(s1, x0+185, y0+5);
	

// trace faisceau localizer
	float alpha1 = 90.0-liste_bali[num_bali].orient_ILS; // angle direct
	if (alpha1<0)  {alpha1 += 360.0;}
	
	float alpha2 = alpha1 + 180.0;
	if (alpha2>360)  {alpha2 -= 360.0;} // angle opposé
	

	affi_rayon2(357, 80, 2, 150, alpha2-3.0, NOIR, 0);
	affi_rayon2(357, 80, 2, 150, alpha2+3.0, NOIR, 0);
	

	refresh_carte=0;

	x_avion=0;
	y_avion=0;
	memo_x_avion=0;

//calcul de l'échelle compte tenu des valeurs mesurées, voir commentaire au-dessus de cette fonction

	px_par_NM = (140.0 / (1 << zoom)) * 1.852; // -> variable globale


// cercle 10NM
	TFT480.setTextFont(1);
	TFT480.setTextColor(NOIR);
	if (zoom>4)
	{
		uint16_t r = 10*(uint16_t)px_par_NM;
		TFT480.drawCircle(357, 80, r, NOIR);
		TFT480.drawString("10",357+3+0.7*r,75+0.7*r);
	}

// cercle 20NM	
	if (zoom>5)
	{
		uint16_t r = 20.0*px_par_NM;
		TFT480.drawCircle(357, 80, r, NOIR);
		TFT480.drawString("20",357+3+0.7*r,75+0.7*r);
	}
	
// cercle 30NM	
	if (zoom>6)
	{
		uint16_t r = 40.0*px_par_NM;
		TFT480.drawCircle(357, 80, r, NOIR);
		TFT480.drawString("40",357+3+0.7*r,75+0.7*r);
	}

// cercle 80NM	
	if (zoom>7)
	{
		uint16_t r = 80.0*px_par_NM;
		TFT480.drawCircle(357, 80, r, NOIR);
		TFT480.drawString("80",357+3+0.7*r,75+0.7*r);
	}

// cercle 160NM	
	if (zoom>8)
	{
		uint16_t r = 160.0*px_par_NM;
		TFT480.drawCircle(357, 80, r, NOIR);
		TFT480.drawString("160",357+3+0.7*r,75+0.7*r);
	}

// cercle 320NM	
	if (zoom>9)
	{
		uint16_t r = 320.0*px_par_NM;
		TFT480.drawCircle(357, 80, r, NOIR);
		TFT480.drawString("320",357+3+0.7*r,75+0.7*r);
	}
	
// cercle 640NM	
	if (zoom>10)
	{
		uint16_t r = 640.0*px_par_NM;
		TFT480.drawCircle(357, 80, r, NOIR);
		TFT480.drawString("640",357+3+0.7*r,75+0.7*r);
	}
	
	affi_avion_sur_carte(cap);
}





void affi_data_frq()
{
	String s1;
	float v1;
// -------------------------------------
// entête
	
	s1= liste_bali[num_bali].ID_OACI;
	TFT480.setFreeFont(FM9);
	cadre_top1.affiche_string(s1);

	s1=(String)(num_bali +1);
	s1+=" ";
	s1+= liste_bali[num_bali].nom;
	if ((s1.length())>19) {TFT480.setTextFont(2);} else {TFT480.setFreeFont(FM9);}
	cadre_top2.affiche_string(s1);


	v1= liste_bali[num_bali].frq_VOR /100.0;
	cadre_00.affiche_float(v1, 2);
	
	s1= liste_bali[num_bali].ID_VOR;
	cadre_10.affiche_string(s1);

	v1= liste_bali[num_bali].frq_ILS /100.0;
	cadre_01.affiche_float(v1, 2);
	
	s1= liste_bali[num_bali].ID_ILS;
	cadre_11.affiche_string(s1);

	TFT480.setTextColor(BLEU, NOIR);
	v1= liste_bali[num_bali].orient_ILS;
	cadre_21.affiche_int(v1);


// affichage du type de balise
	uint8_t v2= liste_bali[num_bali].typ;
	String s2="";
	//s2=(String)v2;
	
	if ((v2 & _LOC) == _LOC) {s2+="LOC ";}
	if ((v2 & _DME) == _DME) {s2+="DME ";}
	if ((v2 & _GS)  == _GS)  {s2+="GS ";}
	if ((v2 & _NDB) == _NDB) {s2+="NDB ";}

	TFT480.setTextFont(1);
	cadre_bas1.affiche_string(s2);
	
	s2="";
	if ((v2 & _LOC) == 0) {s2+="No LOC";}
	if ((v2 & _GS) == 0) {s2+=" No GS";}

	
	TFT480.setTextFont(2);
	cadre_bas2.affiche_string(s2);

	ils_orientation=liste_bali[num_bali].orient_ILS *100.0;

	
	refresh_ND=0;
}


void affi_switches()
{
	TFT480.setTextFont(1);
	TFT480.setTextColor(JAUNE);

	TFT480.drawString(switches, 420, 300);
	//TFT480.drawString("hello", 420, 300); // ok
}



void setup()
{
    Serial.begin(19200);

	WiFi.persistent(false);
	WiFi.begin(ssid, password);
	delay(2000);


	pinMode(bouton1, INPUT);

	//pinMode(led1, OUTPUT);

	pinMode(rot1a, INPUT_PULLUP);
	pinMode(rot1b, INPUT_PULLUP);
	pinMode(rot2a, INPUT_PULLUP);
	pinMode(rot2b, INPUT_PULLUP);


	attachInterrupt(rot1a, rotation1, RISING);
	attachInterrupt(rot2a, rotation2, RISING);

	TFT480.init();
	TFT480.setRotation(1); // 0..3 à voir, suivant disposition de l'afficheur
	//TFT480.setRotation(0);
	
	TFT480.fillScreen(NOIR);

	init_SDcard();

	init_FG_bali();

	init_sprites();

	init_affi_vor();
	

	affi_ND(cap);
	memo_cap=cap;
	dessine_pannel_frq();

	affi_carte();
	
	delay(100);


	TFT480.setTextColor(NOIR, BLANC);

	//TFT480.setFreeFont(FF19);

	delay(100);

	//TFT480.fillRect(48, 0, 6, 100,   0xFFE0);
	
//	TFT480.fillRect(0, 0, 479, 30, NOIR);
	TFT480.setTextColor(BLANC, NOIR);
	TFT480.setFreeFont(FF19);
	String s1 = "FG_RADIO v";
	s1+= version;
//	TFT480.drawString(s1, 70, 3);

	cap=0;

	vor_frq=123500;
	ils_frq=108000;

	vor_dst=1852*102; // 102km
	vor_radial=360.0 *100.0;
	ils_dst=20000; // 20km

	zoom=3;
//	affichages();

	bouton1_etat = digitalRead(bouton1);
	memo_bouton1_etat = bouton1_etat;

	dessine_pannel_frq();
	refresh_carte=1;

	x_avion=250;
	y_avion=20;
	memo_x_avion=0;
	
	//affi_dst_VOR();
	//affi_radial_VOR();
	
	//affi_dst_NAV2();
	//affi_radial_NAV2();

	//affi_data_frq();

		
}


uint16_t L=0;
uint16_t C=0;
uint16_t x, y;
uint16_t i1 = 0;

uint8_t p1;

int32_t number = 0;

String parametre;
String s1;
String s2;


void affichages()
{
	affi_dst_VOR();
	affi_radial_VOR();
	affi_dst_ILS();
	affi_actual_ILS();

	if(cap != memo_cap)
	{
		affi_ND(cap);
		
		refresh_ND=1; // pour raffraichir l'affichage du panneau de fréquences à droite
		memo_cap = cap;
	}

	if (refresh_carte==1)
	{
		affi_carte();
	}
}



void toutes_les_10s()
{
	dessine_pannel_frq(); // donc le cadre s'abime lors de la rotation du prite à gauche
}



void toutes_les_1s()
{

	nb_secondes++;

	if(nb_secondes>10)
	{
		nb_secondes=0;
		toutes_les_10s();
	}

	affi_switches();

}



void interroge_WiFi()
{
// voir les "server.on()" dans la fonction "setup()" du code du PFD pour la source des données
	recp_hdg = "";
	if(WiFi.status()== WL_CONNECTED )
	{
		httpGetHDG(); // envoie requette au serveur
		
		TFT480.setTextColor(VERT, NOIR);
		hdg1=recp_hdg.toInt();

		if(hdg1 != memo1_hdg1)
		{
			refresh_Encoche=1;
			cadre_hdg1.efface();
			cadre_hdg1.affiche_int(hdg1);

		}
		
		httpGetCap();
		cap=recp_cap.toInt();
		if(cap != memo_cap)
		{
			affi_ND(cap);
			refresh_Encoche=1;
			
		}

		httpGetRd1();
		vor_radial=recp_vor.toInt();

		/*
		cadre_rd1.efface();
		cadre_rd1.affiche_int(vor_radial);
		*/
		
		if(vor_radial != memo_vor_radial)
		{
			memo_vor_radial = vor_radial;
			affi_ND(cap);
		}
		
		httpGetVor1nm();
		vor_dst = recp_vor1nm.toInt();

		httpGetILSnm();
		ils_dst = recp_ILSnm.toInt();

		httpGetILSactual(); 
		ils_actual = recp_ILS_actual.toInt();

		//httpGet_SW();
		
	}
	else if (premier_passage == 1)
	{
		RAZ_variables();
		affi_ND(cap);
		////TFT480.setFreeFont(FM9);
		////TFT480.setTextColor(ORANGE, NOIR);
		////TFT480.drawString("Att cnx WiFi", 10, 290);
		////delay(1000);
		premier_passage=0;
	}

}



void toutes_les_100ms()
{
	nb_100ms++;


	if(nb_100ms>10)
	{
		nb_100ms=0;
		toutes_les_1s();
	}

	if (refresh_ND==1)
	{
		affi_data_frq();
		affi_ND(cap);

		refresh_ND=0;
		//refresh_Encoche=1;
	}

	if (refresh_Encoche==1)
	{
		affi_encoche_hdg(memo1_hdg1, NOIR); // efface
		affi_encoche_hdg(hdg1, OLIVE);
		affi_ND(cap);
		
		memo1_hdg1=hdg1;
		refresh_Encoche=0;
	}

	affi_avion_sur_carte(cap);
 }


/** ================================================================== 
 variables à gérer obligatoirement */

uint8_t TEST_AFFI=0;// =0 pour un fonctionnement normal; =1 pour tester les affichages et la fluidité

//le nombre de ports GPIO libres étant atteint, on va utiiser un switch unique pour deux fonctions :
uint8_t fonction_bt1 = 0; // 0=saisie écran ; 1=autoland (atterrissage automatique)

/** ==================================================================  */

uint16_t t=0; // temps -> rebouclera si dépassement    
void loop()
{

//le bouton1 est partagé entre deux fonctions, voir ci-dessus "variables à gérer obligatoirement"
	bouton1_etat = digitalRead(bouton1);

	if (bouton1_etat != memo_bouton1_etat)
	{
		memo_bouton1_etat = bouton1_etat;
		if (bouton1_etat==0)
		{
			TFT480.fillCircle(455, 310, 5, VERT);
			

			if (fonction_bt1 == 0) {write_TFT480_on_SDcard(); }
			if (fonction_bt1 == 1)
			{
				memo2_hdg1 = hdg1; // mémorise le cap avant de passer la main à l'autoland
			}
		}
		if (bouton1_etat==1)
		{
			TFT480.fillCircle(455, 310, 5, NOIR);
			if (fonction_bt1=1)
			{
// si, suite à un enclenchement non correct de l'autoland (sur un mauvais sens du localizer en particulier)	le cap a été boulversé
// on le remet à sa valeur mémorisée. Evite de devoir tourner le bouton pendant une heure avec l'avion qui part à l'aventure !			
				hdg1 = memo2_hdg1; 
			}
		}
	}
	
	temps_ecoule = micros() - memo_micros;

	if (temps_ecoule  >= 1E5)  // (>=) et pas strictement égal (==) parce qu'alors on rate l'instant en question
	{
		memo_micros = micros();
		toutes_les_100ms();
	}
	


 
//---------------------------------------------------------------------------------------
	if(TEST_AFFI==1) // pour test des affichages:
	{  

	// les valeurs de ces paramètres peuvent être modifiées afin de tester tel ou tel point particulier
		
	//vspeed +=16.666;
	//if(vspeed > 120) {vspeed = -100;}
		
	
		cap =180.0+180.0*sin(t/100.0);

		affi_ND(cap); 

		vor_dst-=50;
		vor_radial-=1;
		if (vor_radial <0) {vor_radial=360; }
		
		ils_dst-=30;

		affichages();

		t++;

		
		
		TFT480.setTextColor(JAUNE, BLEU);
		TFT480.setFreeFont(FF1);
		TFT480.drawString("mode TEST", 10, 280);

		String s1= String( cap, 2);
		TFT480.drawString(s1, 10, 300);

		delay(30);
		
	}
	
//---------------------------------------------------------------------------------------
	else  // FONCTIONNEMENT NORMAL
	{
		compteur1+=1;
		if (compteur1 >= 100)  // tous les 1000 passages dans la boucle 
		{
			compteur1=0;
			
		}		

		interroge_WiFi();

		affichages();
		
		delay(30);
		
/** ----------------------------------------------
 pour tester si data_ok à bien la valeur attendue, c.a.d si toutes les acquisitions sont effectuées
 ce qui est le cas lorsque le dialogue avec le serveur de protocole FG est correctement programmé
 sachant que ce ne sont pas les pièges qui manquent !
*/ 
////TFT480.setFreeFont(FM9);
////TFT480.setTextColor(VERT, NOIR);
////TFT480.drawString("d_sum:", 0, 0);
////s2= (String) data_ok;
////TFT480.fillRect(70, 0, 50, 30, NOIR);
////TFT480.drawString(s2, 70, 0);

/** ---------------------------------------------- **/
				
		
	}

//---------------------------------------------------------------------------------------



}


/** ***************************************************************************************
	CLASS Cadre  // affiche un nombre ou un petit texte dans un rectangle
	ainsi que (en plus petit) deux valeurs supplémentaires, par ex: les valeurs mini et maxi					
********************************************************************************************/

// Constructeur
Cadre::Cadre()
{

}



void Cadre::init(uint16_t xi, uint16_t yi, uint16_t dxi, uint16_t dyi)
{
	x0 = xi;
	y0 = yi;
	dx = dxi;
	dy = dyi;

	TFT480.setTextColor(BLANC, NOIR);
}



void Cadre::traceContour()
{
	TFT480.drawRoundRect(x0, y0, dx, dy, 3, couleur_contour);
}



void Cadre::efface()
{
	TFT480.fillRect(x0, y0, dx, dy, NOIR);
	traceContour();
}




void Cadre::affiche_int(uint32_t valeur) 
{
	efface();
	TFT480.setFreeFont(FM9);
	TFT480.setTextColor(couleur_texte, NOIR);
	s1 = String(valeur); 
	//TFT480.fillRect(x0, y0, 52, 18, NOIR); // efface
	TFT480.drawString(s1, x0+5, y0+5);
}



void Cadre::affiche_float(float valeur, uint8_t nb_dec) 
{
	efface();
	TFT480.setFreeFont(FM9);
	TFT480.setTextColor(couleur_texte, NOIR);
	s1 = String( valeur, nb_dec); 
	//TFT480.fillRect(x0, y0, 52, 18, NOIR); // efface
	TFT480.drawString(s1, x0+5, y0+5);

}



void Cadre::affiche_string(String txt) 
{
// Pour cette méthode, la police de caract doit être choisie au préalable, extérieurement
	efface();
	TFT480.setTextColor(couleur_texte, NOIR);
	s1 = txt; 
	TFT480.drawString(s1, x0+5, y0+5);
}



