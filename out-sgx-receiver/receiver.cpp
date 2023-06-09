#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <cstring>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>

//  Common C++ header files
#include <SGX_Vole_Define.h>

//  for sgx process
#include <occlum_pal_api.h>
#include <linux/limits.h>

int main(int argc, char *argv[]) {

    
    /*-------------------------- Receiver Protocol -----------------------------

        out TEE
            --generate rsa-pk & sk
            --receiver pk' from Sender (we can transfer n/d/e or pk_pem)
        in TEE
            --given m,F,B,pk,pk', 
            --run VOLE
            --Encrypt(AC, pk)       **(might omit)
            --Encrypt(BΔ, pk')
        out TEE
            --decrypt Encrypt(AC, pk)    **(might omit)
            --send Encrypt(BΔ, pk') to Sender

    --------------------------------------------------------------------------*/

    std::cout << "--------------------------------------------------" << endl;
    Timer totalBegin = std::chrono::system_clock::now();

    int PROTOCOL_MODE  = (int) strtoul(argv[1], NULL, 10);
        // =0	A/C are not needed to encrypt
        // =1	A/C are needed to encrypt

    int HYBRID_ENCRYPTION_ON = (int) strtoul(argv[2], NULL, 10);
        // =0	hybrid encryption is off
        // =1	hybrid encryption is on

    
    
    /*----------- Generate Key -----------*/
    RSA *receiver_rsa;
    BIGNUM *receiver_bne;
    RSA *receiver_pk;

    if (PROTOCOL_MODE){

        std::cout << "Generating RSA-key for Receiver ..." << endl;
        Timer generatepkBegin = std::chrono::system_clock::now();

        receiver_rsa = RSA_new();
        receiver_bne = BN_new();
        BN_set_word(receiver_bne, RSA_PBULIC_EXPONENT);
        RSA_generate_key_ex(receiver_rsa, RSA_KEY_BIT_LENGTH, receiver_bne, NULL);
        
        // RSAPrivateKey ::= SEQUENCE {
        //     version           Version,
        //     modulus           INTEGER,  -- n
        //     publicExponent    INTEGER,  -- e
        //     privateExponent   INTEGER,  -- d
        //     prime1            INTEGER,  -- p
        //     prime2            INTEGER,  -- q
        //     exponent1         INTEGER,  -- d mod (p-1)
        //     exponent2         INTEGER,  -- d mod (q-1)
        //     coefficient       INTEGER,  -- (inverse of q) mod p
        //     otherPrimeInfos   OtherPrimeInfos OPTIONAL
        // }

        receiver_pk = RSAPublicKey_dup(receiver_rsa);
        std::cout << "Generating RSA-key for Receiver done ";
        Timer generatepkEnd = std::chrono::system_clock::now();
        std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(generatepkEnd - generatepkBegin).count() << "ms" << std::endl;
        std::cout << "--------------------------------------------------" << endl;


        //  encrypt: rsa or pk could both be used
        //  decrypt: rsa or sk could both be used
    }

    /*----------- Socket Transfer -----------*/

    Timer connectBegin = std::chrono::system_clock::now();
    std::cout << "Socket Transfering ..." << endl;

    //  create socket
    int receiver_sockfd;
    if( (receiver_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
    {
        std::cout << "---[Error] Create socket failed." << endl;
        return -1;
    }
    else std::cout << "---Create socket" << endl;

    //  bind receiver's port & addr to socket
    struct sockaddr_in receiver_addr;
    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons((int)RECEIVER_PORT);        //  Sender Port
    receiver_addr.sin_addr.s_addr = inet_addr(RECEIVER_ADDR);     //  Sender IP

    if( bind(receiver_sockfd, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr)) != 0 )
    {
        std::cout << "---[Error] Bind socket failed." << endl;
        return -1;
    }
    else std::cout << "---Bind socket" << endl;

    //  listen: transfer this default-"sender-like" socket to a "listener-like" socket
    if( listen(receiver_sockfd, DEFAULT_RECEIVER_BACKLOG) != 0 )
    {
        std::cout << "---[Error] Listen failed." << endl;
        close(receiver_sockfd);
        return -1;
    }
    else std::cout << "---Listen" << endl;


    //  wating for the connection from the Sender

    socklen_t socklen=sizeof(struct sockaddr_in);
    struct sockaddr_in sender_addr;
    int sender_sockfd = accept(receiver_sockfd, (struct sockaddr*)&sender_addr, (socklen_t *)&socklen);
    if(sender_sockfd < 0) std::cout << "---[Error] Connect from Sender failed." << endl;
    else std::cout << "---Connect from Sender " << inet_ntoa(sender_addr.sin_addr) <<  " ";

    Timer connectEnd = std::chrono::system_clock::now();
    // std::cout << "---[Time] Socket Connection: ";
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(connectEnd - connectBegin).count() << "ms" << std::endl;
    
    //  receiver n&e from the Sender
        //  send buffer strlen(buff)
        //  recv buffer sizeof(buff)


    
    Timer receivepkBegin = std::chrono::system_clock::now();

    int iret;

    //  re-construct n&e from the recv-buffer
        //  we have char_buff_n & char_buff_e sequencial in the memory
        //  so the strlen(char_buff_n) will count (char_buff_e) in
        //  so once receive, consturct BIGNUM


    BIGNUM *sender_n = BN_new();
    BIGNUM *sender_e = BN_new();
    char char_buff_sender_n[CHAR_BUFF_SIZE];
    char char_buff_sender_e[CHAR_BUFF_SIZE];
    memset(char_buff_sender_n, 0, sizeof(char_buff_sender_n));
    memset(char_buff_sender_e, 0, sizeof(char_buff_sender_e));

    //  receive n 
    if ( (iret = recv(sender_sockfd, char_buff_sender_n, sizeof(char_buff_sender_n), 0)) <= 0 ) // receive n from Sender
    { 
        std::cout << "---[Error] Receive Sender-pk(n) failed." << endl;
        return -1;
    }
    else std::cout << "---Receive Sender-pk(n) " << endl;
    BN_hex2bn(&sender_n, char_buff_sender_n);
    

    //  receive e 
    if ( (iret = recv(sender_sockfd, char_buff_sender_e, sizeof(char_buff_sender_e), 0)) <= 0 ) // receive e from Sender
    { 
        std::cout << "---[Error] Receive Sender-pk(e) failed." << endl;
        return -1;
    }
    else std::cout << "---Receive Sender-pk(e)" << endl;
    BN_hex2bn(&sender_e, char_buff_sender_e);

    
    Timer receivepkEnd = std::chrono::system_clock::now();
    std::cout << "Socket Transfering done ";
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(receivepkEnd - receivepkBegin).count() << "ms" << std::endl;
    
    //  re-construct sender_pk from n&e
    RSA *sender_pk = RSA_new();
    RSA_set0_key(sender_pk, BN_dup(sender_n), BN_dup(sender_e), NULL); //  must set NULL here for pk
    
    std::cout << "--------------------------------------------------" << endl;



    /*----------- Connect with TEE process -----------*/

    // Init Occlum PAL
    occlum_pal_attr_t pal_attr = OCCLUM_PAL_ATTR_INITVAL;
    pal_attr.instance_dir = "occlum_instance";
    pal_attr.log_level = "off";
    if (occlum_pal_init(&pal_attr) < 0) {
        return EXIT_FAILURE;
    }

    
    /* --------------  Prepare cmd path and arguments -----------------
        cmd_path,
        protocol_mode       //  Protocol mode
        B,                  //  VOLE-para B
        F,                  //  VOLE-para F
        m                   //  VOLE-para m
        
        sender_pk,              //  sender_pk 
        share_buf_B,            //  shared_buf_ptr
        share_buf_B,            //  shared_buf_size
        share_buf_Delta,        //  shared_buf_ptr
        share_buf_Delta,        //  shared_buf_size

        share_buf_A,            //  shared_buf_ptr
        share_buf_A,            //  shared_buf_size
        share_buf_C,            //  shared_buf_ptr
        share_buf_C,            //  shared_buf_size

        (Optional)
        receiver_pk,            //  receiver_pk
    ----------------------------------------------------------------- */


    //  cmd_path   
    const char *cmd_path = "/bin/vole";     //  in-sgx app name

    //  B F m 
    char space_for_field_B[DEFAULT_PARA_LENGTH];
    char space_for_field_F[DEFAULT_PARA_LENGTH];
    char space_for_size_m[DEFAULT_PARA_LENGTH];

    sprintf(space_for_field_B, "%d",(int)FIELD_B);
    sprintf(space_for_field_F, "%d",(int)FIELD_F);
    sprintf(space_for_size_m, "%d",(int)SIZE_M);

    const char *field_B_str = (const char*)space_for_field_B;   //  unsigned long   use BIGNUM* to present the big type
    const char *field_F_str = (const char*)space_for_field_F;   //  unsigned long
    const char *size_m_str = (const char*)space_for_size_m;     //  unsigned long   here is for the n = 1000000


    //  protocol_mode
    int OVERALL_PROTOCOL_MODE = 2*HYBRID_ENCRYPTION_ON + PROTOCOL_MODE;
    //      HYBRID_ENCRYPTION_ON    PROTOCOL_MODE
    //  0       0                          0
    //  1       0                          1
    //  2       1                          0
    //  3       1                          1


    char space_for_overall_proto_mode[DEFAULT_PARA_LENGTH];
    sprintf(space_for_overall_proto_mode, "%d", OVERALL_PROTOCOL_MODE);
    const char *overall_protocol_mode_str = (const char*)space_for_overall_proto_mode;



    //  sender-pk
    char sender_pk_str[32] = {0};               //  RSA*
    snprintf(sender_pk_str, sizeof (sender_pk_str), "%lu", (unsigned long)sender_pk);

    //  buffer A/B/C/Delta Common Paras
    // The buffer shared between the outside and inside the enclave
    // the data encrypted in the enclave could be stored in shared_buff
    int byte_length_field_B = (int)(FIELD_B/8);
    int byte_length_field_F = (int)(FIELD_F/8);
    int bytes_count_B = byte_length_field_F;
    int bytes_count_Delta = byte_length_field_B;

    //  buffer B/Delta  -> ptr & size
    int sender_pk_size = RSA_size(sender_pk);       //  count in byte
    int element_B_count_per_cipher = (sender_pk_size   - DEFAULT_PADDING_LENGTH) / bytes_count_B;
    int cipher_count_B = ceil(SIZE_M *1.0/ element_B_count_per_cipher);
    
    int buffer_size_B = cipher_count_B * sender_pk_size;                //  RSA:    buffer_B_size = ciphertext
    if(HYBRID_ENCRYPTION_ON) buffer_size_B = SIZE_M * bytes_count_B;    //  Hybrid: buffer_B_size = plaintext (ciphertext = plaintext)

    unsigned char *buffer_B = (unsigned char*)malloc(buffer_size_B);


    char share_buf_B_ptr_str[32] = {0};
    char share_buf_B_size_str[32] = {0};
    snprintf(share_buf_B_ptr_str, sizeof(share_buf_B_ptr_str), "%lu", (unsigned long) buffer_B);
    snprintf(share_buf_B_size_str, sizeof(share_buf_B_size_str), "%lu", sizeof(buffer_B));


    int buffer_size_Delta = sender_pk_size;                                                     //  RSA:    buffer_Delta_size = 1*ciphertext
    if(HYBRID_ENCRYPTION_ON) buffer_size_Delta = max(bytes_count_Delta, AES_BLOCK_SIZE_BYTE);   //  Hybrid: buffer_Delta_size = at least 1*plaintext

    unsigned char *buffer_Delta = (unsigned char*)malloc(buffer_size_Delta);
    char share_buf_Delta_ptr_str[32] = {0};
    char share_buf_Delta_size_str[32] = {0};
    snprintf(share_buf_Delta_ptr_str, sizeof(share_buf_Delta_ptr_str), "%lu", (unsigned long) buffer_Delta);
    snprintf(share_buf_Delta_size_str, sizeof(share_buf_Delta_size_str), "%lu", sizeof(buffer_Delta));


    //  buffer A/C      -> ptr & size               //  different mode, different size

    int bytes_count_A = byte_length_field_B;
    int bytes_count_C = byte_length_field_F;    

    int buffer_size_A = SIZE_M * bytes_count_A;    //  default mode-0  plaintext transfer between receiver & enclave
    int buffer_size_C = SIZE_M * bytes_count_C;


    //  for PROTOCOL_MODE = 0, no need to encrypt A/C
    char receiver_pk_str[32] = {0};     //  RSA*
    int receiver_pk_size;               //  count in byte
    int element_A_count_per_cipher;
    int element_C_count_per_cipher;
    int cipher_count_A;
    int cipher_count_C;

 
    //  for PROTOCOL_MODE = 1
    if (PROTOCOL_MODE){
        receiver_pk_size = RSA_size(receiver_pk);   //  count in byte
        element_A_count_per_cipher = (receiver_pk_size - DEFAULT_PADDING_LENGTH) / bytes_count_A;
        element_C_count_per_cipher = (receiver_pk_size - DEFAULT_PADDING_LENGTH) / bytes_count_C;
        cipher_count_A = ceil(SIZE_M *1.0/ element_A_count_per_cipher);
        cipher_count_C = ceil(SIZE_M *1.0/ element_C_count_per_cipher);
        buffer_size_A = cipher_count_A * receiver_pk_size;
        buffer_size_C = cipher_count_C * receiver_pk_size;

        //  receiver-pk
        snprintf(receiver_pk_str, sizeof (receiver_pk_str), "%lu", (unsigned long)receiver_pk);

        if(HYBRID_ENCRYPTION_ON){
            buffer_size_A = SIZE_M * bytes_count_A;
            buffer_size_C = SIZE_M * bytes_count_C;
        }
    }


    unsigned char *buffer_A = (unsigned char*)malloc(buffer_size_A);
    unsigned char *buffer_C = (unsigned char*)malloc(buffer_size_C);


    char share_buf_A_ptr_str[32] = {0};
    char share_buf_A_size_str[32] = {0};
    snprintf(share_buf_A_ptr_str, sizeof(share_buf_A_ptr_str), "%lu", (unsigned long) buffer_A);
    snprintf(share_buf_A_size_str, sizeof(share_buf_A_size_str), "%lu", sizeof(buffer_A));

    char share_buf_C_ptr_str[32] = {0};
    char share_buf_C_size_str[32] = {0};
    snprintf(share_buf_C_ptr_str, sizeof(share_buf_C_ptr_str), "%lu", (unsigned long) buffer_C);
    snprintf(share_buf_C_size_str, sizeof(share_buf_C_size_str), "%lu", sizeof(buffer_C));


    //  for HYBRID_ENCRYPTION_ON = 1, store key & iv for sender & receiver
    unsigned char* AES_buffer_sender = (unsigned char*)malloc(4*sender_pk_size);
    unsigned char* AES_buffer_receiver = (unsigned char*)malloc(4*receiver_pk_size);
    char AES_buffer_sender_ptr_str[32] = {0};
    char AES_buffer_sender_size_str[32] = {0};
    char AES_buffer_receiver_ptr_str[32] = {0};
    char AES_buffer_receiver_size_str[32] = {0};


    if(HYBRID_ENCRYPTION_ON){
        //  for AES_buffer_sender
        snprintf(AES_buffer_sender_ptr_str, sizeof(AES_buffer_sender_ptr_str), "%lu", (unsigned long) AES_buffer_sender);
        snprintf(AES_buffer_sender_size_str, sizeof(AES_buffer_sender_size_str), "%lu", sizeof(AES_buffer_sender));
        
        if(PROTOCOL_MODE){
            //  for AES_buffer_receiver
            snprintf(AES_buffer_receiver_ptr_str, sizeof(AES_buffer_receiver_ptr_str), "%lu", (unsigned long) AES_buffer_receiver);
            snprintf(AES_buffer_receiver_size_str, sizeof(AES_buffer_receiver_size_str), "%lu", sizeof(AES_buffer_receiver));
        }
    }



    //  cmd_args
    //  an array of type (const char*) for cmd arguments 


    //  PROTOCOL_MODE = 0           Not to encrypt A/C
    //  HYBRID_ENCRYPTION_ON = 0    Not use AES for B/Delta
    const char *cmd_args0[] = {  
        cmd_path,                       //  cmd_path            0
        overall_protocol_mode_str,      //  Protocol mode       1
        field_B_str,                    //  VOLE-para B         2
        field_F_str,                    //  VOLE-para F         3
        size_m_str,                     //  VOLE-para m         4
        
        sender_pk_str,                  //  sender_pk           5
        share_buf_B_ptr_str,            //  shared_buf_ptr      6
        share_buf_B_size_str,           //  shared_buf_size     7
        share_buf_Delta_ptr_str,        //  shared_buf_ptr      8
        share_buf_Delta_size_str,       //  shared_buf_size     9

        share_buf_A_ptr_str,            //  shared_buf_ptr      10
        share_buf_A_size_str,           //  shared_buf_size     11
        share_buf_C_ptr_str,            //  shared_buf_ptr      12
        share_buf_C_size_str,           //  shared_buf_size     13
        NULL
    };


    //  PROTOCOL_MODE = 1           Need to encrypt A/C
    //  HYBRID_ENCRYPTION_ON = 0    Not use AES for B/Delta
    const char *cmd_args1[] = {  
        
        cmd_path,                       //  cmd_path            0
        overall_protocol_mode_str,      //  Protocol mode       1
        field_B_str,                    //  VOLE-para B         2
        field_F_str,                    //  VOLE-para F         3
        size_m_str,                     //  VOLE-para m         4 
        
        sender_pk_str,                  //  sender_pk           5
        share_buf_B_ptr_str,            //  shared_buf_ptr      6
        share_buf_B_size_str,           //  shared_buf_size     7
        share_buf_Delta_ptr_str,        //  shared_buf_ptr      8
        share_buf_Delta_size_str,       //  shared_buf_size     9

        share_buf_A_ptr_str,            //  shared_buf_ptr      10
        share_buf_A_size_str,           //  shared_buf_size     11
        share_buf_C_ptr_str,            //  shared_buf_ptr      12
        share_buf_C_size_str,           //  shared_buf_size     13

        receiver_pk_str,                //  receiver_pk         14
        NULL
    };

    //  PROTOCOL_MODE = 0           Not to encrypt A/C
    //  HYBRID_ENCRYPTION_ON = 1    Need use AES for B/Delta
    const char *cmd_args2[] = {  

        cmd_path,                       //  cmd_path            0
        overall_protocol_mode_str,      //  Protocol mode       1   
        field_B_str,                    //  VOLE-para B         2
        field_F_str,                    //  VOLE-para F         3
        size_m_str,                     //  VOLE-para m         4
        
        sender_pk_str,                  //  sender_pk           5
        share_buf_B_ptr_str,            //  shared_buf_ptr      6
        share_buf_B_size_str,           //  shared_buf_size     7
        share_buf_Delta_ptr_str,        //  shared_buf_ptr      8
        share_buf_Delta_size_str,       //  shared_buf_size     9

        share_buf_A_ptr_str,            //  shared_buf_ptr      10
        share_buf_A_size_str,           //  shared_buf_size     11
        share_buf_C_ptr_str,            //  shared_buf_ptr      12
        share_buf_C_size_str,           //  shared_buf_size     13

        AES_buffer_sender_ptr_str,      //  AES_buffer_sender_ptr   14
        AES_buffer_sender_size_str,     //  AES_buffer_sender_size  15
        NULL
    };

    //  PROTOCOL_MODE = 1           Need to encrypt A/C
    //  HYBRID_ENCRYPTION_ON = 1    Need use AES for A/B/C/Delta
    const char *cmd_args3[] = {  

        cmd_path,                       //  cmd_path            0
        overall_protocol_mode_str,      //  Protocol mode       1
        field_B_str,                    //  VOLE-para B         2
        field_F_str,                    //  VOLE-para F         3
        size_m_str,                     //  VOLE-para m         4
        
        sender_pk_str,                  //  sender_pk           5      
        share_buf_B_ptr_str,            //  shared_buf_ptr      6
        share_buf_B_size_str,           //  shared_buf_size     7
        share_buf_Delta_ptr_str,        //  shared_buf_ptr      8
        share_buf_Delta_size_str,       //  shared_buf_size     9

        share_buf_A_ptr_str,            //  shared_buf_ptr      10
        share_buf_A_size_str,           //  shared_buf_size     11
        share_buf_C_ptr_str,            //  shared_buf_ptr      12
        share_buf_C_size_str,           //  shared_buf_size     13

        receiver_pk_str,                //  receiver_pk         14

        AES_buffer_sender_ptr_str,      //  AES_buffer_sender_ptr       15
        AES_buffer_sender_size_str,     //  AES_buffer_sender_size      16
        AES_buffer_receiver_ptr_str,    //  AES_buffer_receiver_ptr     17
        AES_buffer_receiver_size_str,   //  AES_buffer_receiver_size    18
        NULL
    };

    struct occlum_stdio_fds io_fds = {
        .stdin_fd = STDIN_FILENO,
        .stdout_fd = STDOUT_FILENO,
        .stderr_fd = STDERR_FILENO,
    };

    // Use Occlum PAL to create new process
    int libos_tid = 0;
    struct occlum_pal_create_process_args create_process_args = {
        .path = cmd_path,
        //  .argv = cmd_args[OVERALL_PROTOCOL_MODE]
        .argv = OVERALL_PROTOCOL_MODE == 0 ? cmd_args0 : (OVERALL_PROTOCOL_MODE == 1 ? cmd_args1 : (OVERALL_PROTOCOL_MODE == 2 ? cmd_args2 : cmd_args3)),
        .env = NULL,
        .stdio = (const struct occlum_stdio_fds *) &io_fds,
        .pid = &libos_tid,
    };

    if (occlum_pal_create_process(&create_process_args) < 0) {
        return EXIT_FAILURE;
    }

    // Use Occlum PAL to execute the cmd
    int exit_status = 0;
    struct occlum_pal_exec_args exec_args = {
        .pid = libos_tid,
        .exit_value = &exit_status,
    };
    if (occlum_pal_exec(&exec_args) < 0) {
        return EXIT_FAILURE;
    }


    /*----------- Decrypt Enc(A/C) Optional -----------*/
    
    int bytes_count_A_total = SIZE_M * bytes_count_A;
    int bytes_count_C_total = SIZE_M * bytes_count_C;


    unsigned char *randA = (unsigned char *)malloc(bytes_count_A_total);
    unsigned char *randC = (unsigned char *)malloc(bytes_count_C_total);

    //  PROTOCOL_MODE = 1           Need to encrypt A/C
    if(PROTOCOL_MODE){  

        std::cout << "Decrypt Enc(A/C) ..." << endl;
        Timer decryptBegin, decryptEnd;
        RSA* receiver_sk = RSAPrivateKey_dup(receiver_rsa);

        //  HYBRID_ENCRYPTION_ON = 1    Need use AES for A/B/C/Delta
            // 1    RSA decrypt to get AES key
            // 2    AES decrypt A/C
        if(HYBRID_ENCRYPTION_ON){

            AES_KEY receiver_aes_key1, receiver_aes_key2;

            unsigned char aes_receiver_key_buffer1[AES_KEY_LENGTH_BYTE];
            unsigned char aes_receiver_key_buffer2[AES_KEY_LENGTH_BYTE];
            unsigned char aes_receiver_ivec1[AES_IV_LENGTH_BYTE];
            unsigned char aes_receiver_ivec2[AES_IV_LENGTH_BYTE];

            memset(aes_receiver_key_buffer1, 0, AES_KEY_LENGTH_BYTE);
            memset(aes_receiver_key_buffer2, 0, AES_KEY_LENGTH_BYTE);
            memset(aes_receiver_ivec1, 0, AES_IV_LENGTH_BYTE);
            memset(aes_receiver_ivec2, 0, AES_IV_LENGTH_BYTE);
            
            //  RSA decrypt
            RSA_private_decrypt(receiver_pk_size, (unsigned char*)(AES_buffer_receiver + 0*receiver_pk_size), aes_receiver_key_buffer1, receiver_sk, PADDING_MODE);
            RSA_private_decrypt(receiver_pk_size, (unsigned char*)(AES_buffer_receiver + 1*receiver_pk_size), aes_receiver_key_buffer2, receiver_sk, PADDING_MODE);
            RSA_private_decrypt(receiver_pk_size, (unsigned char*)(AES_buffer_receiver + 2*receiver_pk_size), aes_receiver_ivec1, receiver_sk, PADDING_MODE);
            RSA_private_decrypt(receiver_pk_size, (unsigned char*)(AES_buffer_receiver + 3*receiver_pk_size), aes_receiver_ivec2, receiver_sk, PADDING_MODE);

            //  Set AES key
            AES_set_decrypt_key(aes_receiver_key_buffer1, AES_KEY_LENGTH_BIT, &receiver_aes_key1);
            AES_set_decrypt_key(aes_receiver_key_buffer2, AES_KEY_LENGTH_BIT, &receiver_aes_key2);

            //  AES decrypt

            //  Decrypt A
            std::cout << "---Decrypt A ";
            decryptBegin = std::chrono::system_clock::now();
            AES_cbc_encrypt(buffer_A, randA, bytes_count_A_total, &receiver_aes_key1, aes_receiver_ivec1, AES_DECRYPT);
            decryptEnd = std::chrono::system_clock::now();
            std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(decryptEnd - decryptBegin).count() << "ms" << std::endl;

            //  Decrypt C
            std::cout << "---Decrypt C ";
            decryptBegin = std::chrono::system_clock::now();
            AES_cbc_encrypt(buffer_C, randC, bytes_count_C_total, &receiver_aes_key2, aes_receiver_ivec2, AES_DECRYPT);
            decryptEnd = std::chrono::system_clock::now();
            std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(decryptEnd - decryptBegin).count() << "ms" << std::endl;
        }

        //  HYBRID_ENCRYPTION_ON = 0    Need use RSA for A/B/C/Delta
            // 1    RSA decrypt A/C
        else{
            int bytes_ptr;

            //  Decrypt A
            std::cout << "---Decrypt A ";
            decryptBegin = std::chrono::system_clock::now();
            bytes_ptr = 0;
            for(int i=0; i<cipher_count_A; i++){
                int res = RSA_private_decrypt(receiver_pk_size, (unsigned char*)(buffer_A + i*receiver_pk_size), (unsigned char*)(randA + bytes_ptr), receiver_sk, PADDING_MODE);
                bytes_ptr += res;
            }
            decryptEnd = std::chrono::system_clock::now();
            std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(decryptEnd - decryptBegin).count() << "ms" << std::endl;
            
            //  Decrypt C
            std::cout << "---Decrypt C ";
            decryptBegin = std::chrono::system_clock::now();
            bytes_ptr = 0;
            for(int i=0; i<cipher_count_A; i++){
                int res = RSA_private_decrypt(receiver_pk_size, (unsigned char*)(buffer_C + i*receiver_pk_size), (unsigned char*)(randC + bytes_ptr), receiver_sk, PADDING_MODE);
                bytes_ptr += res;
            }
            decryptEnd = std::chrono::system_clock::now();
            std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(decryptEnd - decryptBegin).count() << "ms" << std::endl;
        }

        std::cout << "Decrypt Enc(A/C) done" << endl;
        std::cout << "--------------------------------------------------" << endl;

    }
    //  PROTOCOL_MODE = 0    No need to decrypt. Just copy.
    else{   
        memcpy(randA, buffer_A, bytes_count_A_total);
        memcpy(randC, buffer_C, bytes_count_C_total);
    }



    /*----------- Socket Transfering -----------*/

    std::cout << "Socket Transfering ..." << endl;

    //  Send  Protocol-Mode
    Timer sendresultBegin = std::chrono::system_clock::now();

    char buffer_mode[1] = {(char)(HYBRID_ENCRYPTION_ON)};
    if ( (iret = send(sender_sockfd, buffer_mode, 1, 0)) <= 0 )    
    { 
        std::cout << "---[Error] Send Protocol-Mode failed" << endl;
        return -1;
    }
    else std::cout << "---Send Protocol-Mode" << endl;


    //  Send  Enc(B)
    int total_sent = 0;      // 已发送数据的长度
    while (total_sent < buffer_size_B) {  // 只要还有数据未发送完毕
        int sent = send(sender_sockfd, buffer_B + total_sent, buffer_size_B - total_sent, 0);  // 发送剩余部分
        if (sent == -1) {  // 如果发送失败
            std::cout << "---[Error] Send Enc(B) failed" << endl;
            return -1;
        }
        total_sent += (int)sent;  // 更新已发送长度
    }
    std::cout << "---Send Enc(B)" << endl;

    
    
    //  Send  Enc(Delta)
    if ( (iret = send(sender_sockfd, buffer_Delta, buffer_size_Delta, 0)) <= 0 )    // send Enc(B) to Sender
    { 
        std::cout << "---[Error] Send Enc(Delta) failed" << endl;
        return -1;
    }
    else std::cout << "---Send Enc(Delta)" << endl;

    
    //  HYBRID_ENCRYPTION_ON = 1    Send AES key and iv
    if(HYBRID_ENCRYPTION_ON){
        if ( (iret = send(sender_sockfd, AES_buffer_sender, 4*sender_pk_size, 0)) <= 0 )    //  send AES key and iv
        { 
            std::cout << "---[Error] Send RSA-Enc(AES-key/ivec) failed" << endl;
            return -1;
        }
        else std::cout << "---Send RSA-Enc(AES-key/ivec)" << endl;
    }


    Timer sendresultEnd = std::chrono::system_clock::now();
    std::cout << "Socket Transfering done ";
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(sendresultEnd - sendresultBegin).count() << "ms" << std::endl;
    
    

    std::cout << "--------------------------------------------------" << endl;

    // Destroy Occlum PAL
    occlum_pal_destroy();



    //  free all heap space


    free(buffer_A);
    free(buffer_B);
    free(buffer_C);
    free(buffer_Delta);

    free(randA);
    free(randC);
    free(AES_buffer_sender);
    free(AES_buffer_receiver);



    //  close socket
    close(receiver_sockfd);
    close(sender_sockfd);


    Timer totalEnd = std::chrono::system_clock::now();
    std::cout << "Total time: ";
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(totalEnd - totalBegin).count() << "ms" << std::endl;
    

    /*----------- Done -----------*/

    return exit_status;                 //  According to occlum-exec result to return
}