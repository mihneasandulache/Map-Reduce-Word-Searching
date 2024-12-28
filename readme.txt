Tema 1 APD - Sandulache Mihnea Stefan 332CC
Punctaj local: 84/84
Timp de implementare: 10 ore
--------------------------------------------------------------------------------
    Pentru implementarea functiei de map, am generat un vector de dictionare,
corespunzatoare fiecarui fisier de procesat. Fiecare dictionar contine cuvinte
din fisier si index-ul fisierului. Pentru o distribuire eficienta si dinamica
a thread-urilor, am ales sa folosesc un index comun, care este incrementat in
mutex si care reprezinta index-ul fisierului curent de procesat. In momentul
in care acesta este egal cu numarul de fisiere, ne vom opri pentru ca am terminat
de procesat. Functia de procesare a cuvintelor le construieste caracter cu caracter
pana la intalnirea de spatii de orice fel, eliminand orice caracter non-alfabetic.
    Pentru implementarea functiei de reduce, am folosit un dictionar care contine
26 de chei, corespunzatoare celor 26 de litere mici din alfabet. Initial, am
utilizat structura unordered_map pentru dictionarul final, insa dupa multe incercari,
debugg cu gdb si cu flag-ul de compilare -fsanitize=address, am constatat ca functia
de hash nu este thread-safe, asa ca am ales sa folosesc structura map peste tot in
program. In continuare, am utilizat aceeeasi strategie ca in map cu index-ul comun
pentru agregarea dictionarelor din map intr-unul singur, final. Fiecare thread preia
un fisier si va cauta in intrarea corespunzatoare primei litere cuvantul si il va
adauga. Acesta operatie este realizata asemeanea ConcurrentHashMap-ului din Java,
cu un mutex pentru fiecare litera, pentru a paraleliza cat mai mult. Dupa ce toate
thread-urile au terminat aceasta operatie, fiecare thread ia cate o litera si sorteaza
atat cuvintele in functie de numarul de fisiere si lexicografic, cat si indicii fisierelor
si scrie cuvintele in fisierul corespunzator literei.

