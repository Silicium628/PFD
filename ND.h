#ifndef ND_H
#define ND_H

#include <stdint.h>
#include <TFT_eSPI.h>

#include "SPI.h"


TFT_eSPI TFT480 = TFT_eSPI(); // Configurer le fichier User_Setup.h de la bibliothèque TFT480_eSPI au préalable



/** ***********************************************************************************
		CLASS Cadre  // affiche un nombre ou un petit texte dans un rectangle						
***************************************************************************************/


class Cadre
{
public:

	uint16_t x0; //position
	uint16_t y0;
	uint16_t dx; //dimension
	uint16_t dy;

//couleurs
	uint16_t couleur_contour;
	uint16_t couleur_fond;
	uint16_t couleur_texte;

	
	Cadre();
	
	void init(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy);
	void traceContour();
	void efface();
	void affiche_int(uint32_t valeur);
	void affiche_float(float valeur, uint8_t nb_dec);
	void affiche_string(String txt);
	
	
private:


};





#endif
