#include <stdio.h>
#include <stdlib.h>
#include "..\Tools\Server\users_list.h"
#include "..\Tools\Shared\guid.h"
#include "..\Tools\Shared\types.h"
#include <Windows.h>

void getch();

int main()
{
	for (int i = 0; i < 20; i++)
	{
		guid_t guid = new_guid();
		print_guid(guid);
		Sleep(1000);
		printf("\n");
		//char* serialized = serialize_guid(guid);
		//printf(">> %I64d\n", guid);
		//free(serialized);
	}

	/*list_t list = create();
	add(list, "Sergio Pedri", new_guid(), "127.0.0.1");
	add(list, "Andrea Salvati", new_guid(), "127.0.0.2");
	add(list, "Camil Demetrescu", new_guid(), "127.0.0.3");
	printf("Length: %d\n", get_length(list));
	char* serialized = serialize_list(list);
	puts(serialized);
	//getch(); */
	return 0;
}