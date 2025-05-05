# TEMA 2 APD - Chirimeaca Andrei 332CA

## Overview
In cadrul acestei teme am avut de implementat un program in C++ care "simuleaza" protocolul BitTorrent, folosind biblioteca MPI, controland comportamentul mai multor procese diferite(Tracker si clienti).

## Tracker
Tracker-ul este un singur proces, iar acesta se ocupa cu retinerea de informatii legate de clinetii din "reteaua" pe care o administreaza si de fisierele pe care acestia: 1. le detin si le permit celorlalti clienti sa le descare, 2. doresc sa le descarce de la ceilalti clienti.
In faza incipienta, tracker-ul colecteaza informatii de la clienti, precum:
- fisierele pe care acesstia le detin
- numarul de segmente din care sunt compuse fisierele
- hash-urile segmentelor fiserelor respective
Dupa ce retine aceste informatii, tracker-ul defineste pentru fiecare fisier swarm-ul acestuia(clientii care detin fisierul respectiv)

Dupa ce colecteaza aceste informatii, tracker-ul intra intr-o stare de "bucla", in care asteapta request-uri de la clienti, acestea fiind de mai multe tipuri:
- trimitirea hash-urilor si a swarm-urilor catre clintul ce a initializat request-ul
- updatarea swarm-ului in momentul in care un client downloadeaza un segment dintr-un fisier pe care nu-l detinea inainte
- semnalizarea ca un client a terminat din a mai trimite cereri catre tracker/clineti. In momentul in care toti clientii au trimit aceasta semnalizare, tracker-ul anunta clientii ca pot termina din a asculta cereri de download de alti clienti si isi finalizeaza starea de bucla.

## Client
Clientii citesc din fiserul lor de intrare fisierele pe care acestia le detin, hash-urilor segmentelor acestor fisiere, si fisierele pe care acestia doresc sa le descarce. Dupa, clientii trimit fisierele detinute si hash-urile tracker-ului.

Inainte de a incepe sa trimita cereri de download, clientii trimit o cerere tracker-ului pentru a primi hash-urile fisierelor si swarm-urile acestora. Dupa care, pentru fiecare fisier pe care un client doreste sa-l descarce, clientul consulta swarm-ul fisierului respectiv si incearca sa descarce segmentele, pe rand, de la fiecare peer/seeder aflat in swarm (segmentul 1, de la clientul 1, segmentul 2 de la clientul 2 etc.), asigurandu-se astfel o balansare relativ echitabila (Round-robin scheduling). Clientul ce doreste sa descare un segment trimite o cerere cu hash-ul segmentului unui peer/seeder, acesta verifica daca detine acel segment. Daca da, ii trimite clientului un ACK, iar leecher-ul/peer-ul "descarca" segmentul. Daca nu, leecher-ul/peer-ul va initializa o cerere catre urmatorul peer/seeder din swarm, acest proces repetandu-se pana cand se gaseste un seeder care detine segmentul respectiv.

## Eficienta
Dupa cum am specificat in sectiunea in care detaliez modul in care functioneaza clientii, am folosit Round-Robin pentru a balansa traficul intre clientii, insa asta nu a fost suficient, mai ales in momentul in care aveam leech-uri in "reteaua" mea. Asa ca am venit cu urmatoarea abordare: daca un client este leech, atunci va incepe sa descarce segmentele de fisiere in ordine inversa. Acest lucru ajuta la balansarea traficului in momentul in care un client downloadeaza un fisier in acelasi timp cu un leech. Deoarece leech-ul, probabil, nu are segmentele necesare pentru a le trimite si altor clienti, ceilalti clienti care vor acelasi fisier pe care il vrea si leech-ul vor ajunge sa preia fisierul din alta parte. Insa, daca leech-ul descarca fisierul in ordine inversa, cand un alt client ajunge la descarcare din a doua jumatate a fisierului, leech-ul va detine aceste segmente si va putea sa i le transmita. Din testarea mea proprie, am observat o imbunatatire semnificativa a balansarii traficului in momentul in care am inceput sa folosesc aceasta abordare.