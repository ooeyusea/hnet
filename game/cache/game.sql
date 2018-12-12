CREATE TABLE `account` (
	`userId` varchar(128) not null,
	`zone` int default 0,
	PRIMARY KEY(`userId`)
);

CREATE TABLE `role` (
	`id` bigint not null,
	`userId` varchar(128) not null,
	`zone` int default 0,
	`name` varchar(32) binary not null, 
	PRIMARY KEY(`id`),
	INDEX `index_userId` (`userId`) 
);

CREATE TABLE `global` (
	`key` varchar(64) not null,
	`value` varchar(128) not null,
	PRIMARY KEY(`key`)
);
