#include <stdio.h>
#include <stdlib.h>
#include "..\Tools\Server\users_list.h"
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
	add(list, "Sergio Pedri", new_guid(), "127.0.0.1");
	add(list, "Andrea Salvati", new_guid(), "127.0.0.2");
	add(list, "Camil Demetrescu", new_guid(), "127.0.0.3");
	printf("Length: %d\n", get_length(list));
	char* serialized = serialize_list(list);
	printf("\n\n");
	puts(serialized);
	//getch();
	return 0;
}