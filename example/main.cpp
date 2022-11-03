#include <shared_memory>

struct June {
	June() {

	}

	June(int m, int y)
		: month(m), year(y)
	{ }
	int month = 6;
	int year = 2022;
};

int main(int argc, char *argv[])
{
	shared_memory<June> april1 = make_shared_memory<June>();
	shared_memory<June> april2 = make_shared_memory<June>();


	June tmp;
	tmp.year = 1997;
	april1.commit(tmp);

	tmp = {};
	april2.read(tmp);

	shared_memory<June> april3 = std::move(april2);
	april2 = std::move(april1);

	return 0;
}
