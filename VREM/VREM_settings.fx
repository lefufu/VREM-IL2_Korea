//
// settings for VREM
//
// 2026/01 							Lefuneste.
//
// https://forum.dcs.world/topic/356128-reshade-vr-enhancer-mod-vrem/#comment-5503216

#include "ReShade.fxh"
#include "ReShadeUI.fxh"
//*************************
// DO NOT CHANGE THIS ENTRY, IT IS USED TO TRIGGER THE MOD
uniform float set_default <
	hidden = true;
> = 1.0;
//*************************
//sight
// flag for the section
uniform bool set_debug <
    ui_label = "change hangar UI color";
    ui_tooltip = "change hangar UI color";
    ui_category = "7. Dev.";
> = false;

/*
uniform bool set_clock_hours <
    ui_label = "Display hours needle";
    ui_tooltip = "display the needle for hours in the stopwatch";
    ui_category = "5. Stopwatch.";
> = false;

*/

/*
uniform int cb_test_color <
    ui_type = "combo";
    ui_label = "color for fixed color";
    ui_tooltip = "Liste déroulante d'options";
    ui_category = "3. Debug";
    ui_items = "value 1\0value 2\0value 3\0value 4\0";
> = 0;



uniform bool var_label <
    ui_label = "Label masking";
    ui_tooltip = "label masking";
    ui_category = "5. Misc.";
> = true;

uniform float var_haze_factor <
    ui_type = "slider";
    ui_label = "set haze factor";
    ui_tooltip = "0: no haze, 1: orginal haze";
    ui_category = "5. Misc.";
    ui_min = 0.0;
    ui_max = 1.0;
    ui_step = 0.1;
> = 1.0;

uniform float var_reflection <
    ui_type = "slider";
    ui_label = "set instrument refection";
    ui_tooltip = "0: no reflecion, 1: orginal reflection";
    ui_category = "5. Misc.";
    ui_min = 0.0;
    ui_max = 1.0;
    ui_step = 0.1;
> = 1.0;

uniform bool set_effects <
    ui_label = "technique in VR";
    ui_tooltip = "enable rendering of technique in VR displays";
    ui_category = "6. Effects.";
> = true;
*/
/*
uniform bool var_save_cso <
    ui_label = "save .cso for all shaders";
    ui_tooltip = "save .cso for all shaders at game launch (need restart)";
    ui_category = "7. Debug";
> = true;
*/

/*
//====================================================================
// ReShade GUI Widget Test Technique
// Test de tous les types de widgets GUI disponibles dans ReShade
//====================================================================

// 1. SLIDERS (int et float)
//--------------------------------------------------------------------
uniform float fSliderFloat <
    ui_type = "slider";
    ui_label = "Float Slider";
    ui_tooltip = "Test d'un slider float standard";
    ui_category = "1. Sliders";
    ui_min = 0.0;
    ui_max = 100.0;
    ui_step = 0.1;
	ui_category_closed = true;
> = 50.0;

uniform int iSliderInt <
    ui_type = "slider";
    ui_label = "Integer Slider";
    ui_tooltip = "Test d'un slider entier";
    ui_category = "1. Sliders";
    ui_min = 0;
    ui_max = 10;
    ui_step = 1;
> = 5;

// 2. DRAG (valeurs sans contraintes visuelles strictes)
//--------------------------------------------------------------------
uniform float fDragFloat <
    ui_type = "drag";
    ui_label = "Float Drag";
    ui_tooltip = "Drag pour valeurs float (plus précis qu'un slider)";
    ui_category = "2. Drag Controls";
    ui_min = -1000.0;
    ui_max = 1000.0;
    ui_step = 0.01;
> = 0.0;

uniform int iDragInt <
    ui_type = "drag";
    ui_label = "Integer Drag";
    ui_tooltip = "Drag pour valeurs entières";
    ui_category = "2. Drag Controls";
    ui_min = -100;
    ui_max = 100;
> = 0;

// 3. INPUT (saisie directe de valeurs)
//--------------------------------------------------------------------
uniform float fInputFloat <
    ui_type = "input";
    ui_label = "Float Input";
    ui_tooltip = "Saisie directe de valeur float";
    ui_category = "3. Input Fields";
> = 1.0;

uniform int iInputInt <
    ui_type = "input";
    ui_label = "Integer Input";
    ui_tooltip = "Saisie directe de valeur entière";
    ui_category = "3. Input Fields";
> = 1;

// 4. COLOR PICKER
//--------------------------------------------------------------------
uniform float3 fColor3 <
    ui_type = "color";
    ui_label = "RGB Color";
    ui_tooltip = "Sélecteur de couleur RGB";
    ui_category = "4. Color Pickers";
> = float3(1.0, 0.5, 0.0);

uniform float4 fColor4 <
    ui_type = "color";
    ui_label = "RGBA Color";
    ui_tooltip = "Sélecteur de couleur avec alpha";
    ui_category = "4. Color Pickers";
> = float4(0.0, 0.5, 1.0, 1.0);

// 5. CHECKBOX (booléens)
//--------------------------------------------------------------------
uniform bool bCheckbox <
    ui_label = "Simple Checkbox";
    ui_tooltip = "Case à cocher booléenne";
    ui_category = "5. Checkboxes";
> = true;

uniform bool bToggle <
    ui_label = "Toggle Option";
    ui_tooltip = "Autre option booléenne";
    ui_category = "5. Checkboxes";
> = false;

// 6. COMBO BOX (listes déroulantes)
//--------------------------------------------------------------------
uniform int iComboMode <
    ui_type = "combo";
    ui_label = "Mode Selection";
    ui_tooltip = "Liste déroulante d'options";
    ui_category = "6. Combo Boxes";
    ui_items = "Mode 1\0Mode 2\0Mode 3\0Mode 4\0";
> = 0;

uniform int iComboEffect <
    ui_type = "combo";
    ui_label = "Effect Type";
    ui_tooltip = "Sélection du type d'effet";
    ui_category = "6. Combo Boxes";
    ui_items = "None\0Grayscale\0Sepia\0Invert\0Edge Detect\0";
> = 0;

// 7. RADIO BUTTONS
//--------------------------------------------------------------------
uniform int iRadioQuality <
    ui_type = "radio";
    ui_label = "Quality Setting";
    ui_tooltip = "Boutons radio pour la qualité";
    ui_category = "7. Radio Buttons";
    ui_items = "Low\0Medium\0High\0Ultra\0";
> = 2;

// 8. VECTORS (float2, float3, float4)
//--------------------------------------------------------------------
uniform float2 fVector2 <
    ui_type = "slider";
    ui_label = "Vector 2D";
    ui_tooltip = "Vecteur à 2 composantes";
    ui_category = "8. Vectors";
    ui_min = 0.0;
    ui_max = 1.0;
> = float2(0.5, 0.5);

uniform float3 fVector3 <
    ui_type = "drag";
    ui_label = "Vector 3D";
    ui_tooltip = "Vecteur à 3 composantes";
    ui_category = "8. Vectors";
    ui_min = -10.0;
    ui_max = 10.0;
    ui_step = 0.1;
> = float3(0.0, 0.0, 0.0);

// 9. BUTTON (via combo avec reset)
//--------------------------------------------------------------------
uniform int iResetButton <
    ui_type = "combo";
    ui_label = "Reset All";
    ui_tooltip = "Simulateur de bouton (utiliser combo)";
    ui_category = "9. Buttons & Special";
    ui_items = "Active\0Reset Now\0";
> = 0;

// 10. SPACING & TEXT (catégories et organisation)
//--------------------------------------------------------------------
uniform int iSpacerA <
    ui_type = "radio";
    ui_label = " ";
    ui_text = "=== Section A ===";
    ui_category = "10. Spacing & Organization";
> = 0;

uniform float fTestA <
    ui_type = "slider";
    ui_label = "Test Parameter A";
    ui_category = "10. Spacing & Organization";
    ui_min = 0.0;
    ui_max = 1.0;
> = 0.5;

uniform int iSpacerB <
    ui_type = "radio";
    ui_label = " ";
    ui_text = "=== Section B ===";
    ui_category = "10. Spacing & Organization";
> = 0;

uniform float fTestB <
    ui_type = "slider";
    ui_label = "Test Parameter B";
    ui_category = "10. Spacing & Organization";
    ui_min = 0.0;
    ui_max = 1.0;
> = 0.5;

*/
//====================================================================
// TECHNIQUE
//====================================================================



technique VREM_settings
{

}
