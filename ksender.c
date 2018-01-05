#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10000
pachet packageInit,receive;
int numberSequence = 0;
void initPackageS() // Functie care initializeaza un pachet de tipul S
{

    packageInit.SOH = 0x01;
    packageInit.LEN = 0x10; // 16
    packageInit.SEQ = 0x00;
    packageInit.TYPE = 'S';
    packageInit.DATA = malloc(11 * sizeof(char));
    packageInit.DATA[0] = 0xFA; // 250 MAXL
    packageInit.DATA[1] = 0x05; // TIME
    packageInit.DATA[2] = 0x00; // NPAD
    packageInit.DATA[3] = 0x00; //PADC
    packageInit.DATA[4] = 0x0D;  //EOL
    packageInit.DATA[5] = 0x00; //QCTL
    packageInit.DATA[6] = 0x00; //QBIn
    packageInit.DATA[7] = 0x00; //CHKT
    packageInit.DATA[8] = 0x00; //REPT
    packageInit.DATA[9] = 0x00; //CAPA
    packageInit.DATA[10] = 0x00; //R
    packageInit.MARK = 0x0D;
    printf("%d",packageInit.DATA[0]);   
}
void incSeq()
{
    if(numberSequence == 63) // fac modulo 64
    {
        numberSequence = 0;
    }
    else
        numberSequence ++;
}
void send_eot() // Functie care trimite un mesaj de tipul EOT
{

   
    msg t;
    pachet eot;
    t.len = 7;
    eot.SOH = 0x01;
    eot.LEN = 0x05;
    eot.TYPE = 'B';
    incSeq();
    eot.SEQ = numberSequence;
    eot.CHECK = crc16_ccitt(t.payload, t.len - 3); // FARA CAMPUL CHECK SI MARK
    eot.MARK = packageInit.MARK;
    memcpy(t.payload,&eot, 4 * sizeof(char));
    eot.CHECK = crc16_ccitt(t.payload, t.len - 3); 
    memcpy(&(t.payload[4]), &(eot.CHECK), 2 * sizeof(char));
    t.payload[6] = eot.MARK;
    send_message(&t);

}

void send_eof() // Functie care trimite un mesaj de tipul EOF
{
     msg t;
    pachet eof;
    t.len = 7;
    eof.SOH = 0x01;
    eof.LEN = 0x05;
    eof.TYPE = 'Z';
    incSeq();
    eof.SEQ = numberSequence;
    eof.CHECK = crc16_ccitt(t.payload, t.len - 3); // FARA CAMPUL CHECK SI MARK
    eof.MARK = packageInit.MARK;
    memcpy(t.payload,&eof, 4 * sizeof(char));
    eof.CHECK = crc16_ccitt(t.payload, t.len - 3);
    memcpy(&(t.payload[4]), &(eof.CHECK), 2 * sizeof(char));
    
    t.payload[6] = eof.MARK;
    send_message(&t);
}
void send_header(char *argv) // Functie care trimite un mesaj de tipul header
{

    msg t;
    pachet header;
    header.SOH = 0x01;
    incSeq();
    header.SEQ = numberSequence ;
    header.TYPE = 'F';
    int nameDim = strlen(argv);
    t.len =  nameDim + 7;
    header.LEN = t.len - 2;
    header.DATA = malloc(nameDim * sizeof(char));
    memcpy(header.DATA, argv,strlen(argv));
    memcpy(t.payload, &header, 4 * sizeof(char));
    memcpy(&(t.payload[4]), header.DATA, nameDim);
    header.CHECK = crc16_ccitt(t.payload, nameDim + 4); //Fara campul CHECK SI MARK 
    memcpy(&(t.payload[4 + nameDim]), &(header.CHECK), 2 * sizeof(char));
    t.payload[4 + nameDim + 2] = packageInit.MARK;
    send_message(&t);

}


void send_data(char * data,int dim) // Functie care trimite un mesaj de tipul data
{
    msg t;
    pachet dataPack;
    dataPack.SOH = 0x01;
    incSeq();
    dataPack.SEQ = numberSequence;
    dataPack.TYPE = 'D';
    t.len = dim + 7;
    dataPack.LEN = t.len -2;
    dataPack.DATA = malloc(dim * sizeof(char));
    memcpy(dataPack.DATA, data, dim* sizeof(char));
    memcpy(t.payload, &dataPack, 4 * sizeof(char));
    memcpy(&(t.payload[4]), dataPack.DATA, dim);
    dataPack.CHECK = crc16_ccitt(t.payload, dim +4 );
    memcpy(&(t.payload[4 + dim]), &(dataPack.CHECK), 2 * sizeof(char));
    t.payload[4 + dim + 2] = packageInit.MARK;
    send_message(&t);
}
void fill_zero(char * str) // Functie care umple un buffer cu 0-uri
{
    int i;
    for( i=0; i< packageInit.DATA[0]; i++)
        str[i] = 0x00;
}
int main(int argc, char** argv) {
    out = fopen("log.out","w");
    msg t;
    FILE *f;
    int sz, lastPoz = 0, i;
    init(HOST, PORT);
    initPackageS();
    /* Completare payload pentru pachet de tip S.Le-am laut pe bucati pentru ca packageInit.Data este un char *,
    prin urmare memcpy ar fi mutat adresa lui, nu valoarea */
    
    t.len = 18;
    memcpy(t.payload, &packageInit, 4 * sizeof(char)); // Campul SOH, LEN, SEQ, TYPE
    memcpy(&(t.payload[4]),packageInit.DATA, 11 * sizeof(char));
    packageInit.CHECK = crc16_ccitt(t.payload, t.len - 3);// CALCUL CRC
    memcpy(&(t.payload[15]),&(packageInit.CHECK),2 * sizeof(char));  // Includere checkSUM 
    t.payload[17] = t.payload[8];
  
    int ok = 0;
    msg *y;
    //Trimitere pachet de tipul S
    for(i=0 ; i<=2 ; i++)
    {
        printf("\nTrimit pachetul de tip S cu numarul de secventa [%d]\n", numberSequence);
        fflush(stdout);
        send_message(&t);
       
        y = receive_message_timeout((int)(packageInit.DATA[1])  *1000); // Astept cofirmarea pentru cadrul S
        
        if(y != NULL)
        {
           
            if((*y).payload[3] == 'S')
            {
                printf("Am primit ACK-UL pentru cadrul S\n");
                fflush(stdout);
                ok = 1;
                break;
            }
            else //Inseamna ca e NAK

            {
                i = -1; //Resetez contorul pt timeout
                incSeq();
            }
        }


    
    }
    /* TRIMITERE EOT IN CAZUL IN CARE NU SE PRIMESTE DE 3 ORI PACHETUL DE TIP S */
    if(ok == 0)
    {
            printf("A fost trimis de 3 ori pachetul de tip S si nu s-a primit, inchei transmisia");
            send_eot();
            return 0;

    }
    else // Cazul in care am primit raspuns
    {

        memcpy(&receive, (*y).payload, 4);
        if(receive.TYPE == 'D') //Verific daca este end of transmission
        { // Incheiere transmisie daca s-a trimis end of transmision
       
           return 0;
        
        }
        else
        {
            numberSequence = (*y).payload[2]; // Trec la urmatorul pachet, cel cu header
           
        }
    
    }

    // ACUM URMEAZA SA TRIMIT HEADERUL
    int k = 1;
    int ok2 = 1;
    char *str = calloc(packageInit.DATA[0], sizeof(char)); //Marimea maxima a campului DATA

    while(argv[k] != NULL && ok2 == 1) //Aici trebuie sa transmit pachete de tipul Data si Header
    {

        f = fopen(argv[k], "rb"); //Deschid fisierul
        ok = 0;
        for(i = 0 ; i<= 2; i++) // TRIMIT Headerul pana primesc ack
        {
            send_header(argv[k]); // Trimit headerul de 3 ori
            printf("\nFIsierul header cu numarul de secventa [%d] a fost trimis\n",numberSequence);
            fflush(stdout);
            y = receive_message_timeout((int)(packageInit.DATA[1])  *1000);           
            if(y != NULL) //Cazul in care am primit raspuns
            {

               if((*y).payload[3] == 'Y') //verific daca e ack, in caz contrar se intra din nou in for si se retransmite
               {
                    incSeq();
                    printf("\nACK-UL cu numarul de secventa [%d] a fost primit\n", numberSequence);
                    numberSequence--; // Am grija sa incrementez seq mai jos
                    fflush(stdout);
                    ok = 1;
                    break;
               }
               else
               {
                i = -1; //Inseamna ca am primit NACK, resetez contorul
                incSeq(); // Inseamna ca trebuie retrimis pachetul, dar actualizam numberSequence
               }
            }
            numberSequence--; // In cazul in care nu s-a trimis headerul, retrimit cadrul cu nrSequence anterior
        }
        if(ok == 0) //Daca nu s-a trimis de 3 ori
        {
            
        
            fflush(stdout);
            send_eot();
            y = receive_message_timeout((int)(packageInit.DATA[1])  *1000); // Astept ACK-ul pentru EOT
            break;
        
        }
        //Vad cat de mare e fisierul ca sa stiu cand sa ma opresc
        fseek(f, 0L, SEEK_END);
        sz = ftell(f);
        lastPoz = 1;
        fseek(f, 0L, SEEK_SET);
        incSeq(); //Trec la urmatorul pachet, cel de tipul data
        int data = 0;
        while(1) //lopez pana gasesc end of file
        {
            
            fill_zero(str);
            fread(str,1,packageInit.DATA[0],f);
            ok = 0;
            for(i = 0; i <= 2; i++) //Trimit pachetul cu numarul de ordine numberSequence
            {
                incSeq();
                printf("\nTrimit pachetul %d de tipul data cu numarul de ordine [%d]", data, numberSequence);
                numberSequence --; //Scad unul, pentru ca incrementez numberSequence in send_data
                fflush(stdout);   
                send_data(str, ftell(f)- lastPoz +1);   
                y = receive_message_timeout((int)(packageInit.DATA[1])  *1000);  //Daca primesc mesaj
                if(y != NULL)
                {
                    if((*y).payload[3] == 'Y' ) //Verific daca e ack, in caz contrar se intra din nou in if si se retransmite
                    {
                        printf("\nAm primti ACK-ul pentru pachetul %d de tip data cu numarul de ordine[%d]", data, numberSequence);
                        fflush(stdout);
                        ok = 1;
                        break;
                    }
                    else
                    {
                        incSeq();
                        i = -1; //Inseamna ca am primit NACK, resetez contorul
                    }
                }
                
                    
                numberSequence--;// In cazul in care nu se primeste ACK, inseamna ca s-a primit ori NAK, ori timeout, deci trebuie rtransmis
            }
            if(ok == 0) //Daca nu s-a primit raspunde 3 ori, inchei transmisia
            {
                printf("\nPacketul de tip data a fost trimis de 3 ori fara succes");
                send_eot();
                y = receive_message_timeout((int)(packageInit.DATA[1])  *1000); // Astept ACK pt end of transmision
                ok2 = 0;
                break;
            }
            
            lastPoz = lastPoz + packageInit.DATA[0]; 
            if(lastPoz  > sz) //DACA AM AJUNS LA FINAL
                break;

            data ++;
        }
        if(ok2 != 0)
        {
        fflush(stdout);
        ok = 0;
        for(i=0; i<= 2; i++) //Aici transmit un fisier de tipul EOF
        {
      
        
            printf("\nA fost trimis un fisier, trec la urmatorul");
            fflush(stdout);
            send_eof(); // TRIMIT EOF
            y = receive_message_timeout((int)(packageInit.DATA[1])  *1000); // Astept cofirmarea pentru cadrul S
            if(y != NULL)
            {
                if((*y).payload[3] == 'Y')
                {
                    ok = 1;
                    break;
                }
                else
                    i = -1; //Inseamna ca am primit NACK, resetez contorul
            }
        numberSequence--;
        }
        numberSequence--;
        if(ok == 0) 
        {
            printf("\nS-a ajuns la sfarsit");
            fflush(stdout);
            send_eot();
            y = receive_message_timeout((int)(packageInit.DATA[1])  *1000); // Astept ACK pt end of transmision
            break;
        }
        k++;
        fclose(f);

        }
}
printf("\nS-a ajuns la sfarsit");
fflush(stdout);
send_eot();
y = receive_message_timeout((int)(packageInit.DATA[1])  *1000); // Astept ACK pt end of transmision

    
    return 0; 
}
