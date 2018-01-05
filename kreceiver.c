#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "lib.h"
#define HOST "127.0.0.1"
#define PORT 10001
/*
Structura pachet S
payload[0] = SOH
payload[1] = LEN
payload[2] = SEQ
payload[3] = TYPE
De aici incepe data(cei 11 bytes)
payload[15] = Check
payload[17] = MARK
*/

int nrSequence = 0;

pachet pachetInit;
msg initS;
void initPackageS(char *payload)
{

    //Le-am luat separat pt ca nu am putut sa dau memcpy la toata strucura.Campul Data este un char*, si ar fi luat adresa
    memcpy(&pachetInit, payload, 4*sizeof(char));
    pachetInit.DATA = malloc(11 * sizeof(char));
    memcpy(pachetInit.DATA, &(payload[4]), 11 * sizeof(char));
    memcpy(&pachetInit.CHECK, &(payload[15]), 2 * sizeof(char));
    memcpy(&pachetInit.MARK, &(payload[17]), 1 * sizeof(char));

}
void incSeqq()
{
    if(nrSequence == 63)
        nrSequence = 0;
    else
        nrSequence ++;
}

int PackageS() // Functie care verifica daca pachetul de tip S este ok, iar in caz afirmativ apeleaza functia de initializare initPackageS, si returneaza 1
{
    recv_message(&initS);
    unsigned short cks = crc16_ccitt(initS.payload, initS.len -3);// CALCUL CRC
    unsigned short check;
    memcpy(&check, &(initS.payload[15]), 2 * sizeof(char));
    if(cks == check && initS.payload[2] == nrSequence) // Verific daca corespunde seq si daca nrSequence corespunde 
    {
        printf("Pachetul cu numarul de ordine [%d] a fost primit, trimit ack-ul cu numarul de ordine [%d]\n", nrSequence, nrSequence +1);
        fflush(stdout);
        initPackageS(initS.payload);
        incSeqq();
        initS.payload[2] = nrSequence;
        send_message(&initS);
        return 1; //  pachetul este ok            
    }
    printf("Pachetul a fost compromis, trebuie retrimis");
    fflush(stdout);
    
   return 0;
}
void send_EOT() //Functie care trimite un pachet de tipul EOT
{
    msg T;
    pachet EOT;
    T.len = 0x07;
    EOT.SOH = 0x01;
    EOT.LEN = 0x05;
    incSeqq(); 
    EOT.SEQ = nrSequence;
    nrSequence--;
    EOT.TYPE = 'B';
    EOT.CHECK = crc16_ccitt(T.payload, T.len - 3); // FARA CAMPUL CHECK SI MARK
    EOT.MARK = pachetInit.MARK;
    memcpy(T.payload,&EOT, 4 * sizeof(char));
    memcpy(&T.payload[4], &EOT.CHECK, 2 * sizeof(char));
    T.payload[6] = EOT.MARK;
    send_message(&T);
}
void send_NAK() //Functie care trimite un pachet de tipul NAK
{
  
    msg T;
    pachet NAK;
    T.len = 0x07;
    NAK.SOH = 0x01;
    NAK.LEN = 0x05;
    incSeqq(); 
    NAK.SEQ = nrSequence;
    NAK.TYPE = 'N';
    NAK.CHECK = crc16_ccitt(T.payload, T.len -3);
    NAK.MARK = pachetInit.MARK;
    memcpy(T.payload,&NAK, 4 * sizeof(char));
    memcpy(&T.payload[4], &NAK.CHECK, 2 * sizeof(char));
    T.payload[6] = NAK.MARK;
    send_message(&T);
}
void send_ACK() // Functie care trimite un pachet de tipul ACK
{
  
    msg T;
    pachet ACK;
    T.len = 0x07;
    ACK.SOH = 0x01;
    ACK.LEN = 0x05;
    incSeqq();

    ACK.SEQ = nrSequence;
    
    ACK.TYPE = 'Y';
    ACK.CHECK = crc16_ccitt(T.payload, T.len -3);
    ACK.MARK = pachetInit.MARK;
    memcpy(T.payload,&ACK, 4 * sizeof(char));
    memcpy(&T.payload[4], &ACK.CHECK, 2 * sizeof(char));
    T.payload[6] = ACK.MARK;
    send_message(&T);
}

int verifData(msg verif)// FUnctie care verifica daca Checksum-ul este corect
{
    
    unsigned short cks = crc16_ccitt(verif.payload, verif.len -3);
    unsigned short check;
    memcpy(&check, &(verif.payload[verif.len - 3]), 2 * sizeof(char));
    if(cks != check)
        return 0;
    else
        return 1;
}
int verifEnd(msg verif) //Functie care verifica daca pachetul este de tipul EndofTransmision
{
    if(verif.payload[3] == 'B' )
        return 1;
    return 0;
}
int verifEndOfFile(msg verif) //Functie care verifica daca pachetul este de tipul EndOfFile 
{
    if(verif.payload[3] == 'Z')
        return 1;
    return 0;
}

int main(int argc, char** argv) {
    msg  mesaj;
    init(HOST, PORT);
    int i;
    int wait_for_header = 1;
    i = 0;
    int ok = PackageS();
    
    if(ok == 0) // Daca nu s-a primit mesajul de tip PackageS mai astept inca 2 mesaje(2* TIME)
    {
    
        int ok = PackageS();
        if(ok == 0)
             ok = PackageS();
   
	}
    
    if(ok == 0) //Inseamna ca nu s-a trimis de 3 ori 
    {
        recv_message(&mesaj); // Primesc end of transmision
        send_ACK();   
        return 0;
    }
    incSeqq(); //Inc caz contrar, trec la urmatorul mesaj

    if(ok == 1)// Asta inseamna ca s-a primit mesajul de tip S, deci urmeaza sa il primim pe cel de tip header si data
    {
        
        FILE *h; 
        char * fileNameRecv = malloc(20* sizeof(char)); char *fileName = malloc(20*sizeof(char));
  
        while(1) // Lopez pana primesc end of transmision
        {
            recv_message(&mesaj); //Primesc mesaj
            if(verifEnd(mesaj) == 1) // Verific daca e un mesaj de tipul end of transmision
            {
                printf("\n S-a ajuns la sfarsit");
                fflush(stdout);
                if(verifData(mesaj) == 1) //Verific daca nu cumva a fost modificat bitul in "D"
                {
                    send_ACK(); //TRIMIT ACK PT END OF TRANSMISION
                    return 0; //Ma opresc
                }
                else
                {
                    send_NAK(); //Trimit NACK pt End of Transmision
                    nrSequence--;
                }
               
            }
            else// Daca nu e end of tranmision
            {


                if(verifEndOfFile(mesaj) == 1) // Verific daca e end of file
                {
         
        
                    if(verifData(mesaj) == 1) //Verific daca nu cumva a fost modificat bitul de enfofFile
                    {
                        wait_for_header = 1; // Daca da, inseamna ca astept la urmatorul pachet un pachet cu header
                        fclose(h);
                        send_ACK();
                    }
                    else
                    {
                        nrSequence--;
                        send_NAK();
                        
                    }
                  
                }
                else
                {
                      
                    if(wait_for_header == 1) // Daca e un mesaj de tipul header
                    {
                        
                        if(verifData(mesaj) == 1) // Verific daca data este corecta
                        {
                            incSeqq();
                            printf("Headerul cu numarul de ordine [%d] a fost primit, trimit ACK-ul cu numarul de ordine[%d]", nrSequence-1, nrSequence);
                            nrSequence--; //Scad neSequence, pentru ca il adun in sendACK
                            fflush(stdout);
                            memcpy(fileName, &(mesaj.payload[4]), mesaj.len - 7);
                            wait_for_header = 0;
                            strcpy(fileNameRecv, "recv_");
                            strcat(fileNameRecv, fileName);
                            h = fopen(fileNameRecv, "wb"); //Deschif fisierul
                            send_ACK(); // Trimit ACK
                        
                        }
                        else
                        {
                            incSeqq();
                            printf("\nHeaderul a fost corupt, trimit NAK-ul cu numarul de ordine [%d]\n", nrSequence);
                            nrSequence--;
                            fflush(stdout);
                            send_NAK();
                           
                        }
                    }
                    
                    else // Daca nu e de tipul header, inseamna ca e de tipul DATA
                    {
                      
                       
                        if(verifData(mesaj) == 1) // Verific daca datele sunt corecte
                        {
                            incSeqq();
                            printf("\nFisierul de tip data cu numarul de ordine[%d] a fost primit, trimit ACK",nrSequence);
                            fflush(stdout);
                        
                            char *buffer = calloc((mesaj.len - 7) ,sizeof(char));
                            int dim = (mesaj.len - 7);
                            memcpy(buffer, &(mesaj.payload[4]), dim * sizeof(char) );
                          
                          
                            if(buffer[dim -1] == 0x7F) // Am facut asta pentru ca am vazut ca uneori imi pune 7F la final, nu stiu de ce
                                fwrite(buffer,1,dim- 1, h);
                            else
                                fwrite(buffer,1,dim  , h);
                           
                            //Daca fisierul det tip data care a venit are numarul nrSequence, inseamna ca ACK-ul care trebuie trimis trebuie sa aiba nrSequence+1.Incrementarea se face in functia ACK
                            send_ACK();
                            nrSequence--; //Scand nrSeq, pentru ca am adunat in ACK
                            free(buffer);
                        }
                        else
                        {
                          incSeqq();
                          printf("\nFisierul de tip data cu numarul de ordine[%d] a fost corupt, trimit NAK",nrSequence);
                          nrSequence--; //Scad nrSeq, pentru ca adun in ack
                          fflush(stdout);
                          send_NAK();
                         
                        }
                    }
                }
                
            }  
         } 

     }
	return 0;
}
