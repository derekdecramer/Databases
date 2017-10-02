DROP TABLE IF EXISTS Items;
DROP TABLE IF EXISTS Users;
DROP TABLE IF EXISTS Category;
DROP TABLE IF EXISTS Bids;
DROP TABLE IF EXISTS CategoryList;

CREATE TABLE Items(
	ItemID INTEGER PRIMARY KEY,
	SellerID TEXT NOT NULL,
	Name TEXT NOT NULL,
	Currently REAL NOT NULL,
	Buy_Price REAL,
	First_Bid REAL NOT NULL,
	Number_of_Bids INTEGER NOT NULL,
	Starts DATETIME NOT NULL,
	Ends DATETIME NOT NULL CHECK (Ends > Starts),
	Description TEXT,
	FOREIGN KEY(SellerID) REFERENCES User(UserID)
);

CREATE TABLE Users(
	UserID TEXT PRIMARY KEY,
	Rating INTEGER NOT NULL,
	Location TEXT,
	Country TEXT
);

CREATE TABLE Category(
	ItemID INTEGER NOT NULL,
	Category TEXT NOT NULL,
	CONSTRAINT unique_item_category UNIQUE (ItemID, Category),
	PRIMARY KEY (ItemID, Category),
	FOREIGN KEY (Category) REFERENCES CategoryList(Category),
	FOREIGN KEY (ItemID) REFERENCES Item(ItemID)
);

CREATE TABLE Bids(
	UserID TEXT NOT NULL,
	ItemID INTEGER NOT NULL,
	Time DATETIME NOT NULL,
	Amount REAL NOT NULL,
	CONSTRAINT BidID PRIMARY KEY(ItemID, UserID, Amount),
	FOREIGN KEY (UserID) REFERENCES User(UserID),
	FOREIGN KEY (ItemID) REFERENCES Item(ItemID)
);
CREATE TABLE CategoryList(
	Category TEXT PRIMARY KEY
);
