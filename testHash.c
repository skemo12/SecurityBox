#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

unsigned int hash(unsigned char *str)
{
    unsigned int hash = 137;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + (c << 3) + (c << 2) + c; /* hash * 33 + c * 13 */

    return hash;
}
int main()
{
	char abc[5];
	abc[4] = '\0';
	char *v = (char *)calloc(UINT_MAX, sizeof(char));
	int count = 0;
	int tested = 0;
	unsigned int max = 0;
    for (char i = '0'; i <= '9'; i++) {
		for (char j = '0'; j <= '9'; j++)
		{
			for (char k = '0'; k <= '9'; k++)
			{
				for (char l = '0'; l <= '9'; l++)
				{
					abc[0] = i;
					abc[1] = j;
					abc[2] = k;
					abc[3] = l;
					tested++;
					int val = hash(abc);
					if (v[val] != 0)
					{
						printf("DUPLICATE\n");
						count++;
					}
					max = max > val ? max : val;
					v[val] = 1;

				}

			}

		}

	}
	printf("%d\n", tested);
	printf("%u\n", max);
	printf("%u\n", UINT_MAX	);
	printf("%d\n", count);
	printf("Hash of 0000 %u\n", hash("0000"));
	printf("Hash of 1111 %u\n", hash("1111"));
	printf("Hash of 2222 %u\n", hash("2222"));
	printf("Hash of 3333 %u\n", hash("3333"));

    return 0;
}
