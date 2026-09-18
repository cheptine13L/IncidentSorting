# IncidentSorting
Emergency Dispatch System

Sistem de gestionare a intervențiilor de urgență, implementat în limbajul C, care simulează dispecerizarea unităților (poliție, ambulanță, pompieri etc.) către incidente raportate, în funcție de prioritate.

Funcționalități
Gestiunea incidentelor – adăugare incidente cu prioritate (high / medium / low), descriere alocată dinamic și status (queued / intervened / solved)

Dispecerizare (DISPATCH) – alocă automat prima unitate disponibilă celui mai prioritar incident din coadă

Undo (UNDO_LAST_DISPATCH) – anulează ultima intervenție activă, eliberează unitatea și reintroduce incidentul în coadă

Rezolvare incident (SOLVED_INCIDENT) – marchează un incident ca rezolvat și eliberează unitatea alocată

Interogări – informații despre unități, incidente, intervenții active și numărul de unități disponibile



Structuri de date

Structură	Utilizare
Listă dublu-înlănțuită circulară cu sentinelă	Incidente și intervenții

Coadă (FIFO) generică (void *data)	Cozi de prioritate (high/medium/low) și unități disponibile

Stivă (LIFO)	Istoric pentru operația de undo

Memoria pentru descrierile incidentelor este alocată dinamic (malloc) și eliberată complet la finalul execuției (free_system).



Operații suportate

Operație	Argumente	Descriere
ADD_INCIDENT	id priority "descriere"	Adaugă un incident nou
DISPATCH	–	Alocă cel mai apropiat unit disponibil incidentului cel mai prioritar
UNDO_LAST_DISPATCH	–	Anulează ultima intervenție activă
SOLVED_INCIDENT	id	Marchează incidentul ca rezolvat
SHOW_UNIT	id	Afișează detalii despre o unitate
SHOW_INCIDENT	id	Afișează detalii despre un incident
SHOW_INTERVENTIONS	–	Listează toate intervențiile
CHECK_UNITS_AVAILABILITY	–	Afișează numărul de unități disponibile

Fără dependențe externe (doar biblioteca standard: stdio.h, stdlib.h, string.h)
Licență

Acest proiect a fost realizat în scop educațional.
