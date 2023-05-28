# SecurityBox - Paunoiu Darius Alexandru 335CB proiect PM

Scopul este de a creea un "seif" pe baza de cod pin, cu feedback vizual si
auditiv si salvare criptata a parolei pe un card microSD. Codul va fi folosit si
dupa deconectarea de la sursa de energie.

## Implemenetare Hardware

Lista de componente:
- Arduino UNO
- LCD I2C 1604
- Servomotor SG90
- Senzor magnetic HALL KY-003 DIGITAL
- Cititor de micro SD-CARD
- Buton
- Switch intrerupere alimentare
- Buzzer
- Fire

Componentele sunt organizate cat sa incapa intr-o cutie.

## Implementare Software

Codul programului se poate gasi in `proiect.ino` Butonul de reset prin va
actiona o intrerupere. Codul este explicat prin comentarii.

Logica implementarii este destul de simpla, si orice actiune se bazeaza pe un
input de la un utilizator, care este apoi interpretat in loop-ul principal, si
ulterior aplicata actiunea respectiva.

Un cod pin este format din 4 cifre. Nu poate contine alte caractere. Codul pin
de fabrica al aplicatiei este “0000”.

Codurile pin sunt stocate drept hash-uri, nu ca sunt stocate in mod direct.

Codul pin poate fi schimbat prin introducerea secventei “\*00\*”, dupa care se
asteapta codul pin vechi, urmat de introducerea noului codul pin dorit.

In actiune nu se va executa tot codul necesar la resetarea codului pentru ca ar
dura foarte mult, ci in schimb se schimba o variabila de stare ce este
verificata permanent.

Pentru sunetele buzzerului, se folosesc 2 sunete simple, formate din cate 2 note
muzicale. Pentru esec se foloseste de 2 ori nota B4 (494 Hz). Pentru succes, se
folosesc notele B5 (988 Hz) si E6(1319 Hz).

Pentru a gestiona afisarea la LCD, se va folosi o implementare pe stari. Daca
starea device-ului nu s-a schimbat, atunci nu se mai modifica mesajul(nu resetam
lcd-ul degeaba).

## Concluzii

Per total, desi proiectul pleaca de la o idee simpla, implementarea este destul
de complexa, folosindu-se destul de multe mecanisme pentru a “complica” (cu
rost) proiectul. A fost surprizator de greu sa realizez partea fizica a
proiectului (nu ma asteptam sa fie asa greu sa inghesui niste fire intr-o
cutie). Mi-ar fi placut daca aspectul interior al componentelor ar fi fost mai
“clean” dar pentru asta probabil ar fi trebui sa imi leg eu componentele de
placi si sa leg fire exact de lungimea dorita. Consider ca proiectul este destul
de complex atat hardware cat si software.