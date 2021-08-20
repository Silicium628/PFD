
/*---------------------------------------------------------------------------------------
	SW.ino  - Gestion de boutons (switches) pour ESP32
	
	par Silicium628
	Ce logiciel est libre et gratuit sous licence GNU GPL
	
---------------------------------------------------------------------------------------	
	Pour les #includes issus de l'univers Arduino (que je ne fournis pas), il faut voir au cas par cas.
---------------------------------------------------------------------------------------

Notes :

- ce SW envoi toutes les données par WiFi. Il n'est donc pas nécessaire de le connecter
sur un port USB, il peut être simplement alimenté par un adaptateur 5V.
*/

	String version="16.0";
// le numéro de version doit être identique à celui du PFD (Primary Flight Display)



#include <stdint.h>

#include "SPI.h"


#include <WiFi.h>
#include <HTTPClient.h> // Remarque: les fonctions WiFi consomment 50% de la mémoire flash (de programme)

const char* ssid = "PFD_srv"; // nous nous connectons en tant que client au même serveur (le PFD) que le ND.
const char* password = "72r4TsJ28";


//IP address with URL path
const char* srvName_sw = "http://192.168.4.1/switch";

String recp_1;

char var_array[5];	// 4 char + zero terminal - pour envoi par WiFi

uint16_t v_switches=0;

uint32_t memo_micros = 0;
uint32_t temps_ecoule;
uint16_t nb_100ms=0;
uint16_t nb_secondes=0;


uint16_t bouton0=1; // (2^0)
const int GPIO_bouton0 = 12;
bool bouton0_etat;
bool memo_bouton0_etat;

uint16_t bouton1=2; // (2^1)
const int GPIO_bouton1 = 13;
bool bouton1_etat;
bool memo_bouton1_etat;

uint16_t bouton2=4; // (2^2)
const int GPIO_bouton2 = 14;
bool bouton2_etat;
bool memo_bouton2_etat;

uint16_t bouton3=8; // (2^3)
const int GPIO_bouton3= 27;
bool bouton3_etat;
bool memo_bouton3_etat;


//const int led1 = 25;

#define TEMPO 20 // tempo anti-rebond pour l'acquisition des encodeurs rotatifs
volatile uint32_t timer1 = 0;
volatile uint32_t timer2 = 0;

uint16_t compteur1;
uint8_t premier_passage=1;





void httpGet1()
{
	String s1;
	String s2;
// envoie requette au serveur, avec position des switch en argument.
// il s'agit donc ici d'un envoi de données de ce SW vers le PFD. (voir la fonction "setup()" du PFD)

	HTTPClient http;
	char c1[4];

	s1= "?sw1=";  // sw1 (nom arbitraire..) pour désigner l'argument n°1 envoyé dans l'URL de la requête

	s2= String(v_switches);
	
	s1+= s2; // valeur à transmettre
	
	http.begin(srvName_sw+s1);
	int httpResponseCode = http.GET();
	if (httpResponseCode>0)	{ recp_1 = http.getString();}
	http.end();
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
    delay(300);
   // Serial.println("");
   // Serial.println("Ici le SW");

	WiFi.persistent(false);
	WiFi.begin(ssid, password);

	
	delay(100);


	pinMode(GPIO_bouton0, INPUT_PULLUP);
	pinMode(GPIO_bouton1, INPUT_PULLUP);
	pinMode(GPIO_bouton2, INPUT_PULLUP);
	pinMode(GPIO_bouton3, INPUT_PULLUP);

	//pinMode(led1, OUTPUT);

	delay(100);


	bouton0_etat = digitalRead(bouton0);
	memo_bouton0_etat = bouton0_etat;

	bouton1_etat = digitalRead(bouton1);
	memo_bouton1_etat = bouton1_etat;

	bouton2_etat = digitalRead(bouton2);
	memo_bouton2_etat = bouton2_etat;

	bouton3_etat = digitalRead(bouton3);
	memo_bouton3_etat = bouton3_etat;

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



void requette_WiFi()
{
// voir les "server.on()" dans la fonction "setup()" du code du PFD pour la source des données
	recp_1 = "";
	if(WiFi.status()== WL_CONNECTED )
	{
		httpGet1(); // envoie requette au serveur, avec position des switch en argument.
		// il s'agit donc ici d'un envoi de données de ce SW vers le PFD.   
	}
}




void toutes_les_10s()
{
}



void toutes_les_1s()
{
	nb_secondes++;

	if(nb_secondes>10)
	{
		nb_secondes=0;
		toutes_les_10s();
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
}



uint16_t t=0; // temps -> rebouclera si dépassement    
void loop()
{
	bouton0_etat = digitalRead(GPIO_bouton0);
	if (bouton0_etat != memo_bouton0_etat)
	{
		memo_bouton0_etat = bouton0_etat;
		if (bouton0_etat==1) { v_switches &= ~bouton0;} 
		if (bouton0_etat==0) { v_switches |=  bouton0;}

		requette_WiFi();
	}

	bouton1_etat = digitalRead(GPIO_bouton1);
	if (bouton1_etat != memo_bouton1_etat)
	{
		memo_bouton1_etat = bouton1_etat;
		if (bouton1_etat==1) { v_switches &= ~bouton1;}
		if (bouton1_etat==0) { v_switches |=  bouton1;}

		requette_WiFi();
	}

	bouton2_etat = digitalRead(GPIO_bouton2);
	if (bouton2_etat != memo_bouton2_etat)
	{
		memo_bouton2_etat = bouton2_etat;
		if (bouton2_etat==1) { v_switches &= ~bouton2;} 
		if (bouton2_etat==0) { v_switches |=  bouton2;}

		requette_WiFi();
	}

	bouton3_etat = digitalRead(GPIO_bouton3);
	if (bouton3_etat != memo_bouton3_etat)
	{
		memo_bouton3_etat = bouton3_etat;
		if (bouton3_etat==1) { v_switches &= ~bouton3;} 
		if (bouton3_etat==0) { v_switches |=  bouton3;}

		requette_WiFi();
	}	
	
	temps_ecoule = micros() - memo_micros;

	if (temps_ecoule  >= 1E5)  // (>=) et pas strictement égal (==) parce qu'alors on rate l'instant en question
	{
		memo_micros = micros();
		toutes_les_100ms();
	}
	


	compteur1+=1;
	if (compteur1 >= 100)  // tous les 1000 passages dans la boucle 
	{
		compteur1=0;
	}		

	delay(30);
}






