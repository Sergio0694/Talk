#include <stdio.h>
#include <stdlib.h>
#include "..\Tools\Server\users_list.h"
#include "..\Tools\Client\client_list.h"
#include "..\Tools\Shared\guid.h"
#include "..\Tools\Shared\types.h"
#include <Windows.h>

void getch();

int main()
{
	//string_t string = string_concat("ciao", "sergio");
	//printf("%s", string);
	//return 0;

	list_t list = create();
	guid_t guid = new_guid();
	add(list, "Sergio Pedri", new_guid(), "127.0.0.1");
	add(list, "Andrea Salvati", guid, "127.0.0.2");
	add(list, "Camil Demetrescu", new_guid(), "127.0.0.3");
	printf("Length: %d\n", get_length(list));
	char* serialized = serialize_list(list);
	printf("\n\n");
	//puts(serialized);

	deserialize_client_list(serialized, guid);
	//getch();
	return 0;
}